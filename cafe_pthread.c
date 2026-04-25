#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// ================= 시스템 설정 (변경금지) =================

#define M 3             // 대기 차로 크기
#define N 5             // 주문서 레일 크기 
#define K 2             // 바리스타 수
#define TOTAL_CARS 15   // 시뮬레이션할 총 차량 수

// ================= 동기화 변수 및 공유 자원 =================

// 주문구역 변수들
pthread_mutex_t lane_mutex;
sem_t cars_in_lane;   // 대기 중인 차량 수
sem_t taker_ready;    // 접수 직원의 주문 받을 준비 완료 신호
int waiting_cars = 0; // 현재 대기 차로에 있는 차량 수

int lane_queue[M];    // 대기 차로 큐 (필요 시 별도 구현 가능)
int lane_in = 0, lane_out = 0;

// 주방구역 변수들
pthread_mutex_t rail_mutex;
sem_t empty_slots;    // 레일의 빈 공간 수 (초기값 N)
sem_t filled_slots;   // 레일에 걸린 주문서 수 (초기값 0)
int rail_count = 0;   // 현재 레일에 걸린 주문서 수

int rail_queue[N];    // 주문서 레일 큐 (필요 시 별도 구현 가능)
int rail_in = 0, rail_out = 0;

// 현재 시간을 출력하기 위한 헬퍼 함수
void print_time() {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    printf("[%02d:%02d:%02d] ", t->tm_hour, t->tm_min, t->tm_sec);
}

// ================= 스레드 함수 =================

// 1. 차량 고객 스레드
void* car_thread(void* arg) {
    int car_id = *(int*)arg;

    pthread_mutex_lock(&lane_mutex);

    if (waiting_cars >= M) {
        print_time();
        printf("차량 %d: 대기 줄이 너무 길어 그냥 지나갑니다(Drive away).\n", car_id);
        pthread_mutex_unlock(&lane_mutex);
        return NULL;
    }

    lane_queue[lane_in] = car_id;
    lane_in = (lane_in + 1) % M;
    waiting_cars++;

    print_time();
    printf("차량 %d 진입 -> 차로에서 대기합니다. (차로 %d/%d)\n",
           car_id, waiting_cars, M);

    pthread_mutex_unlock(&lane_mutex);

    sem_post(&cars_in_lane);

    return NULL;
}

// 2. 주문 접수 직원 스레드
void* taker_thread(void* arg) {
    while (1) {
        sem_wait(&cars_in_lane);

        pthread_mutex_lock(&lane_mutex);

        int car_id = lane_queue[lane_out];
        lane_out = (lane_out + 1) % M;
        waiting_cars--;

        pthread_mutex_unlock(&lane_mutex);

        print_time();
        printf("주문 접수 직원: 차량 %d 주문서 레일에 등록. (차로 %d/%d)\n",
               car_id, waiting_cars, M);

        sleep(rand() % 2 + 1);

        if (sem_trywait(&empty_slots) != 0) {
            print_time();
            printf("주문 접수 직원: 주문서 레일이 꽉 찼습니다. 대기합니다.\n");
            sem_wait(&empty_slots);
        }

        pthread_mutex_lock(&rail_mutex);

        rail_queue[rail_in] = car_id;
        rail_in = (rail_in + 1) % N;
        rail_count++;

        print_time();
        printf("주문 접수 직원: 차량 %d 주문서 레일에 등록. (레일 %d/%d)\n",
               car_id, rail_count, N);

        pthread_mutex_unlock(&rail_mutex);

        sem_post(&filled_slots);
    }

    return NULL;
}

// 3. 바리스타 스레드
void* barista_thread(void* arg) {
    int barista_id = *(int*)arg;

    while (1) {
        sem_wait(&filled_slots);

        pthread_mutex_lock(&rail_mutex);

        int car_id = rail_queue[rail_out];
        rail_out = (rail_out + 1) % N;
        rail_count--;

        print_time();
        printf("바리스타 %d: 차량 %d 음료 제조 시작. (레일 %d/%d)\n",
               barista_id, car_id, rail_count, N);

        pthread_mutex_unlock(&rail_mutex);

        sem_post(&empty_slots);

        sleep(rand() % 3 + 3);

        print_time();
        printf("바리스타 %d: 차량 %d의 [아이스 아메리카노] 제조 완료!\n",
               barista_id, car_id);
    }

    return NULL;
}

// ================= 메인 함수 (변경 금지) =================
int main() {
    time_t seed = time(NULL);
    srand(seed);
    
    printf("[SEED] 랜덤 검증용 시드값: %lu\n", seed);

    // Mutex 및 세마포어 초기화
    pthread_mutex_init(&lane_mutex, NULL);
    pthread_mutex_init(&rail_mutex, NULL);

    sem_init(&cars_in_lane, 0, 0);   // 초기 차량 0
    sem_init(&taker_ready, 0, 0);    // 초기 직원 비활성
    sem_init(&empty_slots, 0, N);    // 초기 레일 빈자리 N
    sem_init(&filled_slots, 0, 0);   // 초기 레일 주문 0

    pthread_t taker;
    pthread_t baristas[K];
    pthread_t cars[TOTAL_CARS];
    int barista_ids[K];
    int car_ids[TOTAL_CARS];

    // 직원 스레드 생성
    pthread_create(&taker, NULL, taker_thread, NULL);
    
    // 바리스타 스레드 생성
    for (int i = 0; i < K; i++) {
        barista_ids[i] = i + 1;
        pthread_create(&baristas[i], NULL, barista_thread, &barista_ids[i]);
    }

    // 차량 고객 무작위 도착 시뮬레이션 (0.5초 ~ 2초 간격으로 차량 진입)
    for (int i = 0; i < TOTAL_CARS; i++) {
        car_ids[i] = i + 1;
        pthread_create(&cars[i], NULL, car_thread, &car_ids[i]);
        usleep((rand() % 1500 + 500) * 1000);
    }

    // 모든 차량이 진입하고 처리될 때까지 대기
    for (int i = 0; i < TOTAL_CARS; i++) {
        pthread_join(cars[i], NULL);
    }

    // 바리스타가 남은 주문을 모두 처리할 때 까지 대기
    sleep(10);
    print_time();
    printf("오늘의 영업을 종료합니다.\n");

    // 자원 해제
    pthread_mutex_destroy(&lane_mutex);
    pthread_mutex_destroy(&rail_mutex);
    sem_destroy(&cars_in_lane);
    sem_destroy(&taker_ready);
    sem_destroy(&empty_slots);
    sem_destroy(&filled_slots);

    return 0;
}