/**
 * @file simulate_fo_fixed.c
 * @brief Simulate particles using fixed time-step
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
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

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

DECLARE_TARGET
#ifdef SIMD
#pragma omp declare simd uniform(sim)
#endif
real simulate_fo_fixed_inidt(sim_data* sim, particle_simd_fo* p, int i);
void simulate_fo_fixed(particle_queue* pq, sim_data* sim);
//OMP_L1
DECLARE_TARGET_END

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
    /* MAIN SIMULATION LOOP
     * - Store current state
     * - Integrate motion due to background EM-field (orbit-following)
     * - Integrate scattering due to Coulomb collisions
     * - Advance time
     * - Check for end condition(s)
     * - Update diagnostics
     */
    //printf("pqn %d\n", ((int) pq->n/NSIMD + 1)*NSIMD);
//#pragma omp target teams num_teams(NTEAMS) thread_limit(NTHREADS) is_device_ptr(sim, pq, pq_hybrid)
//#pragma omp distribute parallel for simd
//#pragma omp distribute //parallel for 
OMP_L1
//#pragma OMP_L1
//#pragma omp distribute simd
	for(int iprt = 0; iprt < ((int) (pq->n + NSIMD - 1)/NSIMD)*NSIMD ; iprt += NSIMD) 
    //for(int iiprt = 0; iiprt < pq->n; iiprt += NSIMD) 
	//for(int iprt = iiprt; iprt < MIN(iiprt + NSIMD, pq->n); iprt += 1)
	{

#if 0
#ifdef _OPENMP
                                                int ith = omp_get_num_threads();
                                                int tth = omp_get_num_teams();
#else
                                                int ith = 1;
                                                int tth = 1;
#endif
                                                //if (omp_get_team_num() == 0 && omp_get_thread_num() == 0)
                                                //printf("IN PARALLEL REGION 3 RUNNING WITH %d TEAMS AND %d THREADS PER TEAM\n",tth, ith);
#endif



		particle_simd_fo p;  // This array holds current states
		particle_simd_fo p0; // This array stores previous states
        	//printf("%d %d %d\n", omp_get_team_num(), omp_get_thread_num(), iprt);
#if 1

        /* Init dummy markers */
#ifdef SIMD
	#pragma omp simd 
#endif
	//#pragma omp parallel for simd 
	//#pragma omp simd 
OMP_L2
        for(int i=0; i< NSIMD; i++) {
		p.id[i] = -1;
		p.running[i] = 0;
        }

        /* Store marker states */
#ifdef SIMD
        #pragma omp simd
#endif
//OMP_L2
	for(int i = 0; i < NSIMD; i++) {
		if(iprt + i < pq->n) 
		{
			particle_state_to_fo(pq->p[iprt + i], iprt+i, &p, i, &sim->B_data);
			particle_copy_fo(&p, i, &p0, i);
		}
	}

	int n_running = 0;
        real hin[NSIMD]  __memalign__;  // Time step
#if 1
        /* Determine simulation time-step */
#ifdef SIMD
        #pragma omp simd
#endif
OMP_L2
	for(int i = 0; i < NSIMD; i++) 
	{
		int running = p.running[i];
		//printf("*** %d > 0 is %d\n", running, (0 < running));
		if (running > 0) 
		{
			n_running++;
			hin[i] = simulate_fo_fixed_inidt(sim, &p, i);
			//printf("** team %d thread %d iprt %d  n_running = %d\n", omp_get_team_num(), omp_get_thread_num(), iprt, n_running);
		}
	}
	//#if 0
        real cputime, cputime_last; // Global cpu time: recent and previous record
	real diag_time = 0.;
	real step_time = 0.;
	real loop_time = 0.;
	real end_time  = 0.;
	real update_time  = 0.;
        cputime_last   = A5_WTIME;
	//
	loop_time = -A5_WTIME;
        while(n_running > 0) 
	{

            /*************************** Physics **********************************/

            /* Volume preserving algorithm for orbit-following */
	    step_time -= A5_WTIME;
	    step_fo_vpa(&p, hin, &sim->B_data, &sim->E_data);
  	    step_time += A5_WTIME;

            /**********************************************************************/

            /* Update simulation and cpu times */
            //cputime = A5_WTIME;
#ifdef SIMD
#pragma omp simd
#endif
//#pragma omp parallel for 
//OMP_L2
            for(int i = 0; i < NSIMD; i++) 
	    {
                if(p.running[i])
		{
                    p.time[i]     = p.time[i] + hin[i];
                    p.cputime[i] += cputime - cputime_last;
                }
            }
            cputime_last = cputime;

            /* Check possible end conditions */
	    end_time -= A5_WTIME;
            endcond_check_fo(&p, &p0, sim);
	    end_time += A5_WTIME;

            /* Update diagnostics */
            //if(!(sim->record_mode)) 
	    diag_time -= A5_WTIME;
            {
                /* Record particle coordinates */
                diag_update_fo(&sim->diag_data, &p, &p0);
            }
	    diag_time += A5_WTIME;
	    //
            /* Update running particles */
            n_running = 0;
//#pragma omp simd 
	    update_time -= A5_WTIME;
            for(int i = 0; i < NSIMD; i++) 
	    {
                if(p.running[i] > 0) n_running++;
            }
	    update_time += A5_WTIME;
        } // while n_running
	loop_time += A5_WTIME;
	//printf("team %d thread %d loop %f step %f diag %f end %f update %f sum = %f\n", omp_get_team_num(), omp_get_thread_num(), loop_time, step_time, diag_time, end_time, update_time, step_time + diag_time + end_time + update_time);

//#pragma omp simd 
        for(int i = 0; i < NSIMD; i++) 
	{
            if(iprt + i < pq->n) 
	    {
                particle_fo_to_state(&p, i, pq->p[iprt + i], &sim->B_data);
            }
        }
#endif
    } // for iprt
#endif
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
