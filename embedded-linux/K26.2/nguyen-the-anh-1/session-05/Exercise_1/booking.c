#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    int  agent_id;
    char customer[50];
    int  seats_wanted;
} BookingRequest;

BookingRequest requests[5] = {
    {1, "Nguyen Van An",  2},
    {2, "Tran Thi Bich",  1},
    {3, "Le Van Cuong",   3},
    {4, "Pham Thi Dung",  1},
    {5, "Hoang Van Em",   2}
};

int seats_available = 10;
int seats_sold = 0;
int failed_bookings = 0;
pthread_mutex_t seat_lock;

/*
 * Vi sao "kiem tra" va "tru ghe" phai nam trong CUNG MOT khoi lock/unlock:
 *
 * Neu tach thanh 2 lan lock rieng biet (lock -> kiem tra -> unlock,
 * roi lock -> tru -> unlock), se co mot khoang ho giua 2 lan lock do.
 * Trong khoang ho nay, mot luong khac hoan toan co the chen vao, doc
 * thay con du ghe, va tru mat so ghe do truoc. Khi luong ban dau quay
 * lai de tru tiep, no van dang dua tren so lieu "con du ghe" da CU,
 * khong con dung nua -> ket qua la ban vuot so ghe thuc te (race
 * condition). Vi vay "kiem tra + tru" phai duoc coi la MOT hanh dong
 * khong the chia cat (atomic): giu khoa lien tuc tu luc kiem tra cho
 * toi luc tru xong, khong nha khoa giua chung, thi khong luong nao
 * khac co the chen vao pha vo tinh nhat quan cua du lieu.
 */
void *book_ticket(void *arg) {
    BookingRequest *req = (BookingRequest *)arg;

    printf("[Agent %d | TID %lu] Booking %d seats for %s...\n",
           req->agent_id, (unsigned long)pthread_self(),
           req->seats_wanted, req->customer);

    sleep(1);

    pthread_mutex_lock(&seat_lock);
    if (seats_available >= req->seats_wanted) {
        seats_available -= req->seats_wanted;
        seats_sold += req->seats_wanted;
        printf("[Agent %d] CONFIRMED: %d seats for %s. Remaining: %d\n",
               req->agent_id, req->seats_wanted, req->customer, seats_available);
    } else {
        failed_bookings++;
        printf("[Agent %d] SOLD OUT: needs %d seats, only %d left - booking failed.\n",
               req->agent_id, req->seats_wanted, seats_available);
    }
    pthread_mutex_unlock(&seat_lock);

    return NULL;
}

int main() {
    pthread_t agents[5];

    pthread_mutex_init(&seat_lock, NULL);

    printf("==============================================\n");
    printf("   TICKET BOOKING SYSTEM (5 agents, 10 seats)\n");
    printf("==============================================\n");

    for (int i = 0; i < 5; i++) {
        pthread_create(&agents[i], NULL, book_ticket, &requests[i]);
    }

    for (int i = 0; i < 5; i++) {
        pthread_join(agents[i], NULL);
    }

    printf("\n================ SUMMARY ================\n");
    printf("  Total seats     : 10\n");
    printf("  Seats sold      : %d\n", seats_sold);
    printf("  Seats remaining : %d\n", seats_available);
    printf("  Failed bookings : %d\n", failed_bookings);
    printf("=========================================\n");

    pthread_mutex_destroy(&seat_lock);
    return 0;
}
