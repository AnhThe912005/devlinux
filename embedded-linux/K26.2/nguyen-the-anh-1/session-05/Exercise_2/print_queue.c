#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

typedef struct {
    int  doc_id;
    char filename[60];
    int  pages;
} Document;

Document queue[5];
int head = 0, tail = 0, count = 0;
int all_sent = 0;

pthread_mutex_t q_lock;
pthread_cond_t  not_full;
pthread_cond_t  not_empty;

int total_submitted = 0;
int total_printed   = 0;
int total_pages     = 0;

/*
 * Vi sao pthread_cond_wait() phai nam trong while, khong phai if:
 *
 * 1) Nhieu luong cung cho tren cung mot condition variable: khi mot
 *    luong duoc signal/broadcast danh thuc, no phai gianh lai mutex
 *    truoc khi chay tiep. Co the co nhieu luong cung duoc danh thuc
 *    (broadcast), hoac giua luc no thuc day va luc no gianh duoc khoa,
 *    mot luong khac da chen vao va lam dieu kien khong con dung nua
 *    (vi du: hang doi vua day tro lai). Neu dung "if", luong nay se
 *    chay tiep dua tren dieu kien da CU, khong con dung tai thoi diem
 *    hien tai -> ghi/doc sai du lieu. Dung "while" bat buoc kiem tra
 *    lai dieu kien tu dau moi lan thuc day, chi thoat vong lap khi
 *    dieu kien thuc su dung ngay luc do.
 *
 * 2) Spurious wakeup (danh thuc gia): chuan POSIX cho phep he thong
 *    danh thuc mot luong dang pthread_cond_wait() MA KHONG CO bat ky
 *    loi goi signal/broadcast nao, do dac thu cai dat cua he dieu hanh
 *    o tang thap. Neu dung "if", luong se chay tiep nhu the dieu kien
 *    da dung, trong khi thuc ra chang co gi thay doi ca -> loi logic.
 *    Dung "while" dam bao du la thuc day that hay thuc day "gia", code
 *    van luon kiem tra lai dieu kien truoc khi lam bat cu viec gi.
 */

typedef struct {
    int   producer_id;
    char  *doc_names[3];
    int   doc_pages[3];
} ProducerArg;

void *producer(void *arg) {
    ProducerArg *p = (ProducerArg *)arg;

    for (int i = 0; i < 3; i++) {
        pthread_mutex_lock(&q_lock);

        int printed_waiting = 0;
        while (count == 5) {
            if (!printed_waiting) {
                printf("[Producer %d] Queue full - waiting...\n", p->producer_id);
                printed_waiting = 1;
            }
            pthread_cond_wait(&not_full, &q_lock);
        }

        queue[tail].doc_id = p->producer_id * 100 + i;
        snprintf(queue[tail].filename, sizeof(queue[tail].filename), "%s", p->doc_names[i]);
        queue[tail].pages = p->doc_pages[i];
        tail = (tail + 1) % 5;
        count++;
        total_submitted++;

        printf("[Producer %d] Submitting: %-20s (%d pages) - queue: %d/5\n",
               p->producer_id, p->doc_names[i], p->doc_pages[i], count);

        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&q_lock);
    }
    return NULL;
}

void *printer(void *arg) {
    (void)arg;

    while (1) {
        pthread_mutex_lock(&q_lock);

        while (count == 0 && !all_sent) {
            pthread_cond_wait(&not_empty, &q_lock);
        }

        if (count == 0 && all_sent) {
            pthread_mutex_unlock(&q_lock);
            printf("[Printer]    All documents printed. Exiting.\n");
            break;
        }

        Document doc = queue[head];
        head = (head + 1) % 5;
        count--;
        total_printed++;
        total_pages += doc.pages;

        printf("[Printer]    Printing:   %-20s (%d pages) - queue: %d/5\n",
               doc.filename, doc.pages, count);

        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&q_lock);

        sleep(1);
    }
    return NULL;
}

int main() {
    pthread_t producers[3], printer_thread;

    pthread_mutex_init(&q_lock, NULL);
    pthread_cond_init(&not_full, NULL);
    pthread_cond_init(&not_empty, NULL);

    printf("==============================================\n");
    printf("   OFFICE PRINT QUEUE (3 producers, 1 printer)\n");
    printf("   Queue capacity: 5 documents\n");
    printf("==============================================\n\n");

    ProducerArg args[3] = {
        {1, {"report_Q1.pdf", "slides.pdf",   "summary.pdf"}, {12, 20, 4}},
        {2, {"contract.pdf",  "memo.pdf",     "budget.pdf"},  {5, 2, 7}},
        {3, {"invoice.pdf",   "proposal.pdf", "notes.pdf"},   {3, 8, 6}}
    };

    pthread_create(&printer_thread, NULL, printer, NULL);

    for (int i = 0; i < 3; i++) {
        pthread_create(&producers[i], NULL, producer, &args[i]);
    }

    for (int i = 0; i < 3; i++) {
        pthread_join(producers[i], NULL);
    }

    pthread_mutex_lock(&q_lock);
    all_sent = 1;
    pthread_cond_broadcast(&not_empty);
    pthread_mutex_unlock(&q_lock);

    pthread_join(printer_thread, NULL);

    printf("\n================ SUMMARY ================\n");
    printf("  Documents submitted : %d\n", total_submitted);
    printf("  Documents printed   : %d\n", total_printed);
    printf("  Total pages printed : %d\n", total_pages);
    printf("=========================================\n");

    pthread_mutex_destroy(&q_lock);
    pthread_cond_destroy(&not_full);
    pthread_cond_destroy(&not_empty);
    return 0;
}
