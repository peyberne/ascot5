/**
 * @file diag.c
 * @brief Interface for simulation diagnostics
 *
 * Ascot standard output consists of inistate and endstate. Any other output is
 * generated by "diagnostics" that are updated during the simulation. All
 * diagnostics are accessed via this interface. To implement a new diagnostics,
 * it is enough that one add calls to that diagnostics routines here.
 *
 * One limitation for diagnostic data is that the size of the data must be known
 * before simulation begins so that offloading of that data is possible.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ascot5.h"
#include "simulate.h"
#include "B_field.h"
#include "diag.h"
#include "diag/diag_orb.h"
#include "diag/dist_5D.h"
#include "diag/dist_6D.h"
#include "diag/dist_rho5D.h"
#include "diag/dist_rho6D.h"
#include "diag/dist_com.h"
#include "diag/diag_transcoef.h"
#include "particle.h"

void diag_arraysum(int start, int stop, real* array1, real* array2);

/**
 * @brief Initializes offload array from offload data
 *
 * @param data diagnostics offload data
 * @param offload_array pointer to offload array  which is allocated here
 * @param Nmrk number of markers that will be simulated
 *
 * @return zero if initialization succeeded
 */
int diag_init_offload(diag_offload_data* data, real** offload_array, int Nmrk){
    /* Determine how long array we need and allocate it */
    int n = 0;

    if(data->dist5D_collect) {
        data->offload_dist5D_index = n;
        n += data->dist5D.n_r * data->dist5D.n_phi * data->dist5D.n_z
            * data->dist5D.n_ppara * data->dist5D.n_pperp
            * data->dist5D.n_time * data->dist5D.n_q;
    }

    if(data->dist6D_collect) {
        data->offload_dist6D_index = n;
        n += data->dist6D.n_r * data->dist6D.n_phi * data->dist6D.n_z
             * data->dist6D.n_pr * data->dist6D.n_pphi
             * data->dist6D.n_pz * data->dist6D.n_time * data->dist6D.n_q;
    }

    if(data->distrho5D_collect) {
        data->offload_distrho5D_index = n;
        n += data->distrho5D.n_rho * data->distrho5D.n_theta
            * data->distrho5D.n_phi
            * data->distrho5D.n_ppara * data->distrho5D.n_pperp
            * data->distrho5D.n_time * data->distrho5D.n_q;
    }

    if(data->distrho6D_collect) {
        data->offload_distrho6D_index = n;
        n += data->distrho6D.n_rho * data->distrho6D.n_theta
            * data->distrho6D.n_phi
            * data->distrho6D.n_pr * data->distrho6D.n_pphi
            * data->distrho6D.n_pz * data->distrho6D.n_time
            * data->distrho6D.n_q;
    }

    if(data->distCOM_collect) {
        data->offload_distCOM_index = n;
        n += data->distCOM.n_mu * data->distCOM.n_Ekin
            * data->distCOM.n_Ptor;
    }

    data->offload_dist_length = n;

    if(data->diagorb_collect) {
        data->offload_diagorb_index = n;
        data->diagorb.Nmrk = Nmrk;

        switch(data->diagorb.record_mode) {

            case simulate_mode_fo:
                data->diagorb.Nfld = DIAG_ORB_FOFIELDS;
                break;

            case simulate_mode_gc:
                data->diagorb.Nfld = DIAG_ORB_GCFIELDS;
                break;

            case simulate_mode_ml:
                data->diagorb.Nfld = DIAG_ORB_MLFIELDS;
                break;

            case simulate_mode_hybrid:
                data->diagorb.Nfld = DIAG_ORB_HYBRIDFIELDS;
                break;

        }

        if(data->diagorb.mode == DIAG_ORB_POINCARE) {
            n += (data->diagorb.Nfld+2)
                * data->diagorb.Nmrk * data->diagorb.Npnt;
        }
        else if(data->diagorb.mode == DIAG_ORB_INTERVAL) {
            n += data->diagorb.Nfld
                * data->diagorb.Nmrk * data->diagorb.Npnt;
        }
    }

    if(data->diagtrcof_collect) {
        data->offload_diagtrcof_index = n;
        data->diagtrcof.Nmrk = Nmrk;
        n += 3*data->diagtrcof.Nmrk;
    }

    data->offload_array_length = n;
    *offload_array = malloc(n * sizeof(real));
    if(*offload_array == NULL) {
        return 1;
    }

    memset(*offload_array, 0, n * sizeof(real));

    return 0;
}

/**
 * @brief Frees the offload array
 *
 * @param data diagnostics offload data
 * @param offload_array offload array
 */
void diag_free_offload(diag_offload_data* data, real** offload_array) {
    free(*offload_array);
    *offload_array = NULL;
}

/**
 * @brief Initializes diagnostics from offload data
 *
 * @param data diagnostics data
 * @param offload_data diagnostics offload data
 * @param offload_array offload array
 */
void diag_init(diag_data* data, diag_offload_data* offload_data,
               real* offload_array) {
    data->diagorb_collect   = offload_data->diagorb_collect;
    data->dist5D_collect    = offload_data->dist5D_collect;
    data->dist6D_collect    = offload_data->dist6D_collect;
    data->distrho5D_collect = offload_data->distrho5D_collect;
    data->distrho6D_collect = offload_data->distrho6D_collect;
    data->distCOM_collect   = offload_data->distCOM_collect;
    data->diagtrcof_collect = offload_data->diagtrcof_collect;

    if(data->dist5D_collect) {
        dist_5D_init(&data->dist5D, &offload_data->dist5D,
                     &offload_array[offload_data->offload_dist5D_index]);
    }

    if(data->dist6D_collect) {
        dist_6D_init(&data->dist6D, &offload_data->dist6D,
                     &offload_array[offload_data->offload_dist6D_index]);
    }

    if(data->distrho5D_collect) {
        dist_rho5D_init(&data->distrho5D, &offload_data->distrho5D,
                        &offload_array[offload_data->offload_distrho5D_index]);
    }

    if(data->distrho6D_collect) {
        dist_rho6D_init(&data->distrho6D, &offload_data->distrho6D,
                        &offload_array[offload_data->offload_distrho6D_index]);
    }

    if(data->distCOM_collect) {
        dist_COM_init(&data->distCOM, &offload_data->distCOM,
                        &offload_array[offload_data->offload_distCOM_index]);
    }

    if(data->diagorb_collect) {
        diag_orb_init(&data->diagorb, &offload_data->diagorb,
                      &offload_array[offload_data->offload_diagorb_index]);
    }

    if(data->diagtrcof_collect) {
        diag_transcoef_init(
            &data->diagtrcof, &offload_data->diagtrcof,
            &offload_array[offload_data->offload_diagtrcof_index]);
    }
}

/**
 * @brief Free diagnostics data
 *
 * @param data diagnostics data struct
 */
void diag_free(diag_data* data) {
    if(data->diagorb_collect) {
        diag_orb_free(&data->diagorb);
    }
    if(data->diagtrcof_collect) {
        diag_transcoef_free(&data->diagtrcof);
    }
}

/**
 * @brief Collects diagnostics when marker represents a particle
 *
 * @param data diagnostics data struct
 * @param Bdata pointer to magnetic field data
 * @param p_f pointer to SIMD struct storing marker states at the end of current
 *        time-step
 * @param p_i pointer to SIMD struct storing marker states at the beginning of
 *        current time-step
 */
void diag_update_fo(diag_data* data, B_field_data* Bdata, particle_simd_fo* p_f,
                    particle_simd_fo* p_i) {
    if(data->diagorb_collect) {
        diag_orb_update_fo(&data->diagorb, p_f, p_i);
    }

    if(data->dist5D_collect) {
        dist_5D_update_fo(&data->dist5D, p_f, p_i);
    }

    if(data->dist6D_collect) {
        dist_6D_update_fo(&data->dist6D, p_f, p_i);
    }

    if(data->distrho5D_collect) {
        dist_rho5D_update_fo(&data->distrho5D, p_f, p_i);
    }

    if(data->distrho6D_collect) {
        dist_rho6D_update_fo(&data->distrho6D, p_f, p_i);
    }

    if(data->distCOM_collect){
        dist_COM_update_fo(&data->distCOM, Bdata, p_f, p_i);
    }

    if(data->diagtrcof_collect){
        diag_transcoef_update_fo(&data->diagtrcof, p_f, p_i);
    }
}

/**
 * @brief Collects diagnostics when marker represents a guiding center
 *
 * @param data pointer to diagnostics data struct
 * @param Bdata pointer to magnetic field data
 * @param p_f pointer to SIMD struct storing marker states at the end of current
 *        time-step
 * @param p_i pointer to SIMD struct storing marker states at the beginning of
 *        current time-step
 */
void diag_update_gc(diag_data* data, B_field_data* Bdata, particle_simd_gc* p_f,
                    particle_simd_gc* p_i) {
    if(data->diagorb_collect) {
        diag_orb_update_gc(&data->diagorb, p_f, p_i);
    }

    if(data->dist5D_collect){
        dist_5D_update_gc(&data->dist5D, p_f, p_i);
    }

    if(data->dist6D_collect){
        dist_6D_update_gc(&data->dist6D, p_f, p_i);
    }

    if(data->distrho5D_collect){
        dist_rho5D_update_gc(&data->distrho5D, p_f, p_i);
    }

    if(data->distrho6D_collect){
        dist_rho6D_update_gc(&data->distrho6D, p_f, p_i);
    }

    if(data->distCOM_collect){
        dist_COM_update_gc(&data->distCOM, Bdata, p_f, p_i);
    }

    if(data->diagtrcof_collect){
        diag_transcoef_update_gc(&data->diagtrcof, p_f, p_i);
    }
}

/**
 * @brief Collects diagnostics when marker represents a magnetic field line
 *
 * Distributions are not updated for magnetic field lines.
 *
 * @param data pointer to diagnostics data struct
 * @param p_f pointer to SIMD struct storing marker states at the end of current
 *        time-step
 * @param p_i pointer to SIMD struct storing marker states at the beginning of
 *        current time-step
 */
void diag_update_ml(diag_data* data, particle_simd_ml* p_f,
                    particle_simd_ml* p_i) {
    if(data->diagorb_collect) {
        diag_orb_update_ml(&data->diagorb, p_f, p_i);
    }

    if(data->diagtrcof_collect){
        diag_transcoef_update_ml(&data->diagtrcof, p_f, p_i);
    }
}

/**
 * @brief Sum offload data arrays as one
 *
 * The data in both arrays have identical order so distributions can be summed
 * trivially. For orbits and transport coefficients the first array already have
 * space for appending the orbit data from the second array, so we only need to
 * move those elements.
 *
 * @param data pointer to diagnostics data struct
 * @param array1 the array to which array2 is summed
 * @param array2 the array which is to be summed
 */
void diag_sum(diag_offload_data* data, real* array1, real* array2) {
    if(data->diagorb_collect) {
        int arr_start = data->offload_diagorb_index;
        int arr_length = data->diagorb.Nfld * data->diagorb.Nmrk
            * data->diagorb.Npnt;

        memcpy(&(array1[arr_start+arr_length]),
               &(array2[arr_start]),
               arr_length*sizeof(real));
    }

    if(data->diagtrcof_collect) {
        int arr_start = data->offload_diagtrcof_index;
        int arr_length = 3 * data->diagtrcof.Nmrk;

        memcpy(&(array1[arr_start+arr_length]),
               &(array2[arr_start]),
               arr_length*sizeof(real));
    }

    if(data->dist5D_collect){
        int start = data->offload_dist5D_index;
        int stop = start + data->dist5D.n_r * data->dist5D.n_z
                   * data->dist5D.n_ppara * data->dist5D.n_pperp
                   * data->dist5D.n_time * data->dist5D.n_q;
        diag_arraysum(start, stop, array1, array2);
    }

    if(data->dist6D_collect){
        int start = data->offload_dist6D_index;
        int stop = start + data->dist6D.n_r * data->dist6D.n_phi
            * data->dist6D.n_z * data->dist6D.n_pr * data->dist6D.n_pphi
            * data->dist6D.n_pz * data->dist6D.n_time * data->dist6D.n_q;
        diag_arraysum(start, stop, array1, array2);
    }

    if(data->distrho5D_collect){
        int start = data->offload_distrho5D_index;
        int stop = start + data->distrho5D.n_rho * data->distrho5D.n_theta
            * data->distrho5D.n_phi * data->distrho5D.n_ppara
            * data->distrho5D.n_pperp * data->distrho5D.n_time
            * data->distrho5D.n_q;
        diag_arraysum(start, stop, array1, array2);
    }

    if(data->distrho6D_collect){
        int start = data->offload_distrho6D_index;
        int stop = start + data->distrho6D.n_rho * data->distrho6D.n_theta
            * data->distrho6D.n_phi * data->distrho6D.n_pr
            * data->distrho6D.n_pphi * data->distrho6D.n_pz
            * data->distrho6D.n_time * data->distrho6D.n_q;
        diag_arraysum(start, stop, array1, array2);
    }

    if(data->distCOM_collect){
        int start = data->offload_distCOM_index;
        int stop = start + data->distCOM.n_mu * data->distCOM.n_Ekin
            * data->distCOM.n_Ptor;
        diag_arraysum(start, stop, array1, array2);
    }
}

/**
 * @brief Simple helper function for summing elements of two arrays of same size
 *
 * This function is indented for summing distribution ordinates. Indexing starts
 * from 1 and indices given as arguments are inclusive.
 *
 * @param start index to array element where summation begins
 * @param stop index to array element where summation ends
 * @param array1 pointer to array where array2 is summed to
 * @param array2 pointer to array which is to be summed
 */
void diag_arraysum(int start, int stop, real* array1, real* array2) {
    for(int i = start; i < stop; i++) {
        array1[i] += array2[i];
    }
}
