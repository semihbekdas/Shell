#include "model.h"
#include <stdio.h>

int main() {
    // 1. Paylaşılan belleği başlat (model_init)
    ShmBuf *shm = model_init();
    if (!shm) {
        fprintf(stderr, "Shared memory initialization failed.\n");
        return EXIT_FAILURE;
    }
    printf("Shared memory initialized successfully.\n");

    // 2. model_execute_command test: "echo Hello World" komutunu çalıştır.
    char command_output[1024];
    int exit_status = model_execute_command("ls", command_output, sizeof(command_output));
    printf("\n--- Command Execution Test ---\n");
    printf("Exit Status: %d\n", exit_status);
    printf("Command Output:\n%s\n", command_output);

    // 3. model_send_message test: Paylaşılan belleğe mesaj gönder.
    const char *message = "Test message from main";
    shm = model_send_message(shm, "ilk");
    if (!shm) {
        fprintf(stderr, "Sending message failed.\n");
    }
    else {
        printf("\nMessage sent successfully.\n");
    }
    model_send_message(shm, message);
    model_send_message(shm, message);
    model_send_message(shm, "son");
    // 4. model_read_messages test: Gönderilen mesajı paylaşılan bellekten oku.
    char last_message[shm->buf_size];
    int bytes_read = model_read_messages(shm, last_message, sizeof(last_message));
    if (bytes_read < 0) {
        fprintf(stderr, "Reading message failed.\n");
    } else {
        printf("\n--- Message Read Test ---\n");
        printf("Bytes read: %d\n", bytes_read);
        printf("Last message:\n%s\n", last_message);
    }

    // 5. model_cleanup: Paylaşılan belleği ve diğer kaynakları temizle.
    model_cleanup(shm);
    printf("\nShared memory cleaned up.\n");

    return 0;
}
