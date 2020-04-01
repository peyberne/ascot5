/**
 * @file simulate_fo_fixed.c
 * @brief Simulate particles using fixed time-step
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <immintrin.h>
#include <math.h>
#include "../ascot5.h"
#include "../physlib.h"
#include "../simulate.h"
#include "../particle.h"
#include "../wall.h"
#include "../diag.h"
#include "../B_field.h"
#include "../E_field.h"
#include "../plasma.h"
#include "../endcond.h"
#include "../math.h"
#include "../consts.h"
#include "simulate_fo_fixed.h"
#include "step/step_fo_vpa.h"
#include "mccc/mccc.h"

#pragma omp declare target
#pragma omp declare simd uniform(sim)
real simulate_fo_fixed_inidt(sim_data* sim, particle_simd_fo* p, int i);
#pragma omp end declare target

/**
 * @brief Simulates particles using fixed time-step
 *
 * The simulation includes:
 * - orbit-following with Volume-Preserving Algorithm
 * - Coulomb collisions with Euler-Maruyama method
 *
 * The simulation is carried until all markers have met some
 * end condition or are aborted/rejected. The final state of the
 * markers is stored in the given marker array. Other output
 * is stored in the diagnostic array.
 *
 * The time-step is user-defined: either a directly given fixed value
 * or a given fraction of gyrotime.
 *
 * @param pq particles to be simulated
 * @param sim simulation data struct
 */
void simulate_fo_fixed(particle_queue* pq, sim_data* sim) {
    int cycle[NSIMD]  __memalign__; // Flag indigating whether a new marker was initialized
    real hin[NSIMD]  __memalign__;  // Time step

    real cputime, cputime_last; // Global cpu time: recent and previous record

    particle_simd_fo p;  // This array holds current states
    particle_simd_fo p0; // This array stores previous states

    printf("=================== PASSED FO_FIXED 1 =====================\n");


    /* Init dummy markers */
    for(int i=0; i< NSIMD; i++) {
        p.id[i] = -1;
        p.running[i] = 0;
    }

    /* Initialize running particles */
    int n_running = particle_cycle_fo(pq, &p, &sim->B_data, cycle);
    printf("=================== PASSED FO_FIXED 2 =====================\n");

    /* Determine simulation time-step */
    #pragma omp simd
    for(int i = 0; i < NSIMD; i++) {
        if(cycle[i] > 0) {
            hin[i] = simulate_fo_fixed_inidt(sim, &p, i);

        }
    }

    printf("=================== PASSED FO_FIXED 3 =====================\n");

    cputime_last = A5_WTIME;

    /* MAIN SIMULATION LOOP
     * - Store current state
     * - Integrate motion due to background EM-field (orbit-following)
     * - Integrate scattering due to Coulomb collisions
     * - Advance time
     * - Check for end condition(s)
     * - Update diagnostics
     */
    //CLAA
    int ncount = 0;
    while(n_running > 0) {
        printf ("=================== N_RUNNING = %d\n",n_running);
        /* Store marker states */
        #pragma omp simd
        for(int i = 0; i < NSIMD; i++) {
            particle_copy_fo(&p, i, &p0, i);
        }

    //printf("=================== PASSED FO_FIXED 4 =====================\n");
        /*************************** Physics **********************************/

        /* Volume preserving algorithm for orbit-following */
        if(sim->enable_orbfol) {
            step_fo_vpa(&p, hin, &sim->B_data, &sim->E_data);
        }

    //printf("=================== PASSED FO_FIXED 5 =====================\n");
        /* Euler-Maruyama for Coulomb collisions */
        if(sim->enable_clmbcol) {
            mccc_fo_euler(&p, hin, &sim->B_data, &sim->plasma_data,
                          &sim->random_data, &sim->mccc_data);
        }
    //printf("=================== PASSED FO_FIXED 6 =====================\n");

        /**********************************************************************/


        /* Update simulation and cpu times */
        cputime = A5_WTIME;
        #pragma omp simd
        for(int i = 0; i < NSIMD; i++) {
            if(p.running[i]){
                p.time[i] = p.time[i] + hin[i];
                p.cputime[i] += cputime - cputime_last;
            }
        }
        cputime_last = cputime;
    //printf("=================== PASSED FO_FIXED 7 =====================\n");

        /* Check possible end conditions */
        endcond_check_fo(&p, &p0, sim);
    //printf("=================== PASSED FO_FIXED 8 =====================\n");

        /* Update diagnostics */
        if(!(sim->record_mode)) {
            /* Record particle coordinates */
            diag_update_fo(&sim->diag_data, &p, &p0);
        }
        else {
            /* Instead of particle coordinates we record guiding center */

            // Dummy guiding centers
            particle_simd_gc gc_f;
            particle_simd_gc gc_i;

            /* Particle to guiding center transformation */
            #pragma omp simd
            for(int i=0; i<NSIMD; i++) {
                if(p.running[i]) {
                    particle_fo_to_gc( &p, i, &gc_f, &sim->B_data);
                    particle_fo_to_gc(&p0, i, &gc_i, &sim->B_data);
                }
                else {
                    gc_f.id[i] = p.id[i];
                    gc_i.id[i] = p.id[i];

                    gc_f.running[i] = 0;
                    gc_i.running[i] = 0;
                }
            }
            diag_update_gc(&sim->diag_data, &gc_f, &gc_i);
        }
    //printf("=================== PASSED FO_FIXED 9 =====================\n");

        /* Update running particles */
        n_running = particle_cycle_fo(pq, &p, &sim->B_data, cycle);
    //printf("=================== PASSED FO_FIXED 10 =====================\n");

        /* Determine simulation time-step for new particles */
        #pragma omp simd
        for(int i = 0; i < NSIMD; i++) {
            if(cycle[i] > 0) {
                hin[i] = simulate_fo_fixed_inidt(sim, &p, i);
            }
        }
#ifdef CLAA
    int nnn = NSIMD;
    ncount = ncount+1;
    //printf("\n nsimd, n_running : %d %d %d\n\n",nnn,n_running,ncount);
#endif
    //printf("=================== PASSED FO_FIXED 11 =====================\n");

    }

    /* All markers simulated! */

}

/**
 * @brief Calculates time step value
 *
 * The time step is calculated as a user-defined fraction of gyro time,
 * whose formula accounts for relativity, or an user defined value
 * is used as is depending on simulation options.
 *
 * @param sim pointer to simulation data struct
 * @param p SIMD array of markers
 * @param i index of marker for which time step is assessed
 *
 * @return Calculated time step
 */
real simulate_fo_fixed_inidt(sim_data* sim, particle_simd_fo* p, int i) {

    real h;

    /* Value defined directly by user */
    if(sim->fix_usrdef_use) {
        h = sim->fix_usrdef_val;
    }
    else {
        /* Value calculated from gyrotime */
        real Bnorm = math_normc( p->B_r[i], p->B_phi[i], p->B_z[i] );
        real vnorm = math_normc( p->rdot[i], p->phidot[i]*p->r[i], p->zdot[i] );
        real gyrotime = CONST_2PI/
            phys_gyrofreq_vnorm(p->mass[i], p->charge[i], vnorm, Bnorm);
        h = gyrotime/sim->fix_gyrodef_nstep;
    }

    return h;
}
