#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <limits.h>
#include <cblas.h>
#include <math.h>
#include <time.h>

#define array_size 10000 // megethos grammhs kai sthlhs
#define thread_num 4 // arithmos thread
#define k 5 // arithmos k neighours
#define d 1 // diasthsh array
#define THRESHOLD 2500   // orio gia stamathma anadromhs

double *Q_sq;
double *C_sq;
//oi pinakes sto tetragwno

// struct gia na kratame apostaseis kain indexes
typedef struct {
    double dst[k];
    int idx[k];
} KnnResult;

// struct gia anadromikh sunarthsh
typedef struct {
    int start_C;
    int end_C;
    double *Q_point;
    int q_idx;
    double *C;
} RecData;

//dhmiourgia metablhths ThreadData gia to moirasma twn Queries
typedef struct {
    int thread_id; 
    double *C;        
    double *Q;     
    double *knn_dst;  
    int *knn_idx; 
} ThreadData;


// merge neighours kai krataei tis kaluteres apostaseis
void merge_knn(KnnResult left, KnnResult right, KnnResult *final_res) {
    int i = 0, j = 0, count = 0;
    
    // MergeSort gia k stoixeia
    while (count < k) {
        if (left.dst[i] <= right.dst[j]) {
            final_res->dst[count] = left.dst[i];
            final_res->idx[count] = left.idx[i];
            i++;
        } else {
            final_res->dst[count] = right.dst[j];
            final_res->idx[count] = right.idx[j];
            j++;
        }
        count++;
    }
}

// seiriakos upologismos an 
KnnResult compute_direct_knn(int start_C, int end_C, double *Q_point, int q_idx, double *C) {
    KnnResult result;
    for(int m=0; m<k; m++) {
        result.dst[m] = 1e30;
        result.idx[m] = -1;
    }

    int n_C = end_C - start_C;
    double *D_temp = malloc(n_C * sizeof(double));

    //upologismos -2*C*Q^T gia to sugkekrimeno kommati tou Corpus
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasTrans, 
                n_C, 1, d, 
                -2.0, &C[start_C * d], d, 
                Q_point, d, 
                0.0, D_temp, 1);

    for (int i = 0; i < n_C; i++) {
        //upologismos tou sqrt( C^2 - 2*C*Q^T + Q^2 )
        D_temp[i] = sqrt(D_temp[i] + C_sq[start_C + i] + Q_sq[q_idx]);
        
        // grammikos elegxos gia k geitones
        if (D_temp[i] < result.dst[k - 1]) {
            int pos = k - 1;
            while (pos > 0 && D_temp[i] < result.dst[pos - 1]) {
                result.dst[pos] = result.dst[pos - 1];
                result.idx[pos] = result.idx[pos - 1];
                pos--;
            }
            result.dst[pos] = D_temp[i];
            result.idx[pos] = start_C + i; // krata tou index tou Corpus
        }
    }

    free(D_temp);
    return result;
}

// anadromikh divide kai conquer
void* knn_recursive(void* arg) {
    RecData* data = (RecData*)arg;
    KnnResult* my_res = malloc(sizeof(KnnResult));

    int current_size = data->end_C - data->start_C;

    // katw apo mia timh seiriakos upologismos, den aksizei threads
    if (current_size <= THRESHOLD) {
        *my_res = compute_direct_knn(data->start_C, data->end_C, data->Q_point, data->q_idx, data->C);
        return (void*)my_res;
    }

    // Spasimo sth mesh
    int mid = data->start_C + current_size / 2;

    RecData left_data = {data->start_C, mid, data->Q_point, data->q_idx, data->C};
    RecData right_data = {mid, data->end_C, data->Q_point, data->q_idx, data->C};

    pthread_t thread_left;
    
    // aristero miso
    pthread_create(&thread_left, NULL, knn_recursive, &left_data);
    
    // deksi miso
    KnnResult* right_res = (KnnResult*)knn_recursive(&right_data);

    KnnResult* left_res;
    pthread_join(thread_left, (void**)&left_res);

    // merge apotelesmatwn
    merge_knn(*left_res, *right_res, my_res);

    free(left_res);
    free(right_res);
    
    return (void*)my_res;
}

// arxikh sunarthsh
void* knn_neighours(void* arg) {
    ThreadData* data = (ThreadData*)arg;
    
    int id = data->thread_id;
    int block_Q = array_size / thread_num; 
    int total_C = array_size;             
    int start_Q = id * block_Q;

    // loupa gia ta queries na broume ton kontinotero geirona
    for (int j = 0; j < block_Q; j++) {
        int query_idx = start_Q + j;
        double *current_Q_point = &data->Q[query_idx * d];

        RecData root_data = {0, total_C, current_Q_point, query_idx, data->C};
        
        // Kaleitai h anadromikh sunarthsh 
        KnnResult* result = (KnnResult*)knn_recursive(&root_data);

        for(int m=0; m<k; m++) {
            int output_slot = query_idx * k + m; 
            
            data->knn_dst[output_slot] = result->dst[m];
            data->knn_idx[output_slot] = result->idx[m]; 
        }
        free(result);
        //upologismos me anadromh (diairei kai basileue) twn k kontinoterwn
        //geitonwn enos quey me diathrhsh twn index
    }

    return NULL;
}

int main() {
    struct timespec start, finish;
    double elapsed_time;

    C_sq = malloc(array_size * sizeof(double));
    Q_sq = malloc(array_size * sizeof(double));

    double *big_array_C = malloc(array_size * d * sizeof(double));
    double *big_array_Q = malloc(array_size * d * sizeof(double));

    //gemisma me tuxaious arithmous
    for(int i = 0; i < array_size * d; i++) {
        big_array_C[i] = (double)rand() / RAND_MAX; 
        big_array_Q[i] = (double)rand() / RAND_MAX;
    }

    for (int i = 0; i < array_size; i++) {
        double *point_C = &big_array_C[i * d];
        C_sq[i] = cblas_ddot(d, point_C, 1, point_C, 1);
    }

    for (int i = 0; i < array_size; i++) {
        double *point_Q = &big_array_Q[i * d];
        Q_sq[i] = cblas_ddot(d, point_Q, 1, point_Q, 1);
    }

    pthread_t threads[thread_num];
    ThreadData thread_data[thread_num];

    double *final_knn_dst = malloc(array_size * k * sizeof(double));
    int *final_knn_idx = malloc(array_size * k * sizeof(int));

    printf("Start of computing with %d threads...\n", thread_num);
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < thread_num; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].C = big_array_C;
        thread_data[i].Q = big_array_Q;
        thread_data[i].knn_dst = final_knn_dst;
        thread_data[i].knn_idx = final_knn_idx; 

        if (pthread_create(&threads[i], NULL, &knn_neighours, &thread_data[i]) != 0) {
            printf("Error in pthread create %d\n", i);
            return 1;
        }
    }

    for (int i = 0; i < thread_num; i++) {
        pthread_join(threads[i], NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &finish);

    elapsed_time = (finish.tv_sec - start.tv_sec); 
    elapsed_time += (finish.tv_nsec - start.tv_nsec) / 1000000000.0; 

    printf("Success in calculation\n");
    printf("Time in pthread implementation: %.4f seconds\n", elapsed_time);

    // elegxos apotelesmatwn
    printf("\n--- Result checkup in Query 0 ---\n");
    for (int m = 0; m < k; m++) {
        int output_slot = 0 * k + m; 
        printf("Neighour %d -> Distance: %.4f | Index in Corpus: %d\n", 
               m + 1, final_knn_dst[output_slot], final_knn_idx[output_slot]);
    }
    printf("-------------------------------------------\n");

    free(big_array_C);
    free(big_array_Q);
    free(final_knn_dst);
    free(final_knn_idx);
    free(C_sq);
    free(Q_sq);

    return 0;
}
