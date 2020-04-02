/*
 * Copyright (C) 2011-2019 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <assert.h>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include <nanos_omp.h>

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
    	printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

float **A;
float **B;
float **C;

static void convert_to_blocks(unsigned long NB,unsigned long DIM, unsigned long N, float *Alin, float *A[4][4])
{
  unsigned i, j;
  for (i = 0; i < N; i++)
  {
    for (j = 0; j < N; j++)
    {
      //A[i/NB][j/NB][(i%NB)*NB+j%NB] = Alin[j*N+i];
      A[i/NB][j/NB][(i%NB)*NB+j%NB] = Alin[j*N+i];
    }
  }

}

static void print_matrix(float *A[4][4])
{
  int i, j;
  for (i=0; i < 2 ; i++ ) {
    for (j=0; j < 2 ; j++ )
      printf ("%d, %d: %f ", i, j, (*A)[i][j]);
    printf ("\n");
  }
}
static void print_matrix2(float *A)
{
  int i, j;
  for (i=0; i < 2 ; i++ ) {
    for (j=0; j < 2 ; j++ )
      printf ("   %d, %d: %f ", i, j, A[i*128+j]);
    printf ("\n");
  }
}


static double timestemp() {
    struct timeval time;
    if (gettimeofday(&time,NULL)){
        return 0;
    }
    return (double)time.tv_sec + (double)time.tv_usec * .000001;
}

void fill_random(float *Alin, int NN)
{
  int i;
  for (i = 0; i < NN; i++)
  {
    Alin[i]=((float)rand())/((float)RAND_MAX);
  }
}

void init (unsigned long argc, char **argv, unsigned long * N_p, unsigned long * DIM_p)
{
  unsigned int BSIZE=128;
  unsigned long ISEED[4] = {0,0,0,1};
  unsigned long IONE=1;
  unsigned long DIM;
  char UPLO='n';
  float FZERO=0.0;

  DIM=4;

  // matrix init
  unsigned long N=BSIZE*DIM;
  unsigned long NN=N*N;
  int i;

  *N_p=N;
  *DIM_p=DIM;

  // linear matrix
  float *Alin = (float *) malloc(NN * sizeof(float));
  float *Blin = (float *) malloc(NN * sizeof(float));
  float *Clin = (float *) malloc(NN * sizeof(float));

  // fill the matrix with random values
  srand(0);
  fill_random(Alin,NN);
  fill_random(Blin,NN);
  for (i=0; i < NN; i++)
    Clin[i]=0.0;

  A = (float **) malloc(DIM*DIM*sizeof(float *));
  B = (float **) malloc(DIM*DIM*sizeof(float *));
  C = (float **) malloc(DIM*DIM*sizeof(float *));

  for (i = 0; i < DIM*DIM; i++)
  {
     A[i] = (float *) malloc(BSIZE*BSIZE*sizeof(float));
     B[i] = (float *) malloc(BSIZE*BSIZE*sizeof(float));
     C[i] = (float *) malloc(BSIZE*BSIZE*sizeof(float));
  }
  //convert_to_blocks(BSIZE,DIM, N, Alin, (float * [DIM][DIM])A);
  //convert_to_blocks(BSIZE,DIM, N, Blin, (float * [DIM][DIM])B);
  //convert_to_blocks(BSIZE,DIM, N, Clin, (float * [DIM][DIM])C);
  convert_to_blocks(BSIZE,DIM, N, Alin, (float * (*) [4])A);
  convert_to_blocks(BSIZE,DIM, N, Blin, (float * (*) [4])B);
  convert_to_blocks(BSIZE,DIM, N, Clin, (float * (*) [4])C);

  print_matrix((float *(*) [4])C);



  free(Alin);
  free(Blin);
  free(Clin);
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    
    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);


    /* Initialize the enclave */
    if(initialize_enclave() < 0){
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }
 
    /* Utilize edger8r attributes */
    edger8r_array_attributes();
    edger8r_pointer_attributes();
    edger8r_type_attributes();
    edger8r_function_attributes();
    
    /* Utilize trusted libraries */
    ecall_libc_functions();
    ecall_libcxx_functions();
    ecall_thread_functions();


  unsigned long NB, N, DIM;
  struct timeval start;
  struct timeval stop;
  unsigned long elapsed;

  // application inicializations
  init(argc, argv, &N, &DIM);
  NB = 128;

{
  unsigned i, j, k;

  gettimeofday(&start,NULL);
  double s = (double)start.tv_sec + (double)start.tv_usec * .000001;

  for (i = 0; i < DIM; i++)
    for (j = 0; j < DIM; j++)
      for (k = 0; k < DIM; k++) {
#pragma omp task in(A[i][k], B[k][j]) inout(C[i][j]) no_copy_deps
       {
        ecall_matmul_u (global_eid, &A[i][k], &B[k][j], &C[i][j], NB);
        //print_matrix((float *(*) [128])C[i][j]);
        printf ("%d: C block %d %d:\n", nanos_omp_get_thread_num(), i, j);
        print_matrix2(&C[i][j]);
        //float * p = C[0][0];
        //for (i=0; i < 4 ; i++ ) printf ("%f ", p[i]); printf ("\n");
       }
      }

  #pragma omp taskwait
  gettimeofday(&stop,NULL);
  double e =(double)stop.tv_sec + (double)stop.tv_usec * .000001;

  printf("\nMarking starting point.. Timestamp: %f.", s);
  printf("\nMarking starting point.. Timestamp: %f.", e);
  printf("\nInference completed in %f seconds.", (e-s));

  print_matrix2(&C[DIM-1][DIM-1]);
  elapsed = 1000000 * (stop.tv_sec - start.tv_sec);
  elapsed += stop.tv_usec - start.tv_usec;

// threads
#ifdef OMP
  printf("threads: ");
  printf ("%d;\t", omp_get_num_threads() );
#endif
// time in usecs
  printf("time: ");
  printf ("%lu;\t", elapsed);
// performance in MFLOPS
  printf("MFLOPS: %lu\n", (unsigned long)((((float)N)*((float)N)*((float)N)*2)/elapsed));

}


    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);
    
    printf("Info: SampleEnclave successfully returned.\n");

    printf("Enter a character before exit ...\n");
    getchar();
    return 0;
}

