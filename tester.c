#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/wait.h>
#include <pthread.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_PORT 8081  // Verwende Port 8081
#define BUFFER_SIZE 1024

// ANSI-Escape-Sequenzen für Farben
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define CYAN "\033[36m"

// Globale Zähler und Mutex
int total_successful = 0;
int total_failed = 0;
pthread_mutex_t mutex;

int test_get_request(const char *path) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char get_request[BUFFER_SIZE];

    // Erstelle den Socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket konnte nicht erstellt werden");
        return 1; // Fehler beim Erstellen des Sockets
    }

    // Serveradresse konfigurieren
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDR, &server_addr.sin_addr) <= 0) {
        perror("inet_pton fehlgeschlagen");
        close(sock);
        return 1; // Fehler bei der Adressumwandlung
    }

    // Verbinde zum Server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Verbindung zum Server fehlgeschlagen");
        close(sock);
        return 1; // Fehler bei der Verbindung
    }

    // GET Request erstellen
    snprintf(get_request, sizeof(get_request), "GET %s HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n", path);

    // Sende den Request
    send(sock, get_request, strlen(get_request), 0);

    // Antwort vom Server lesen
    int bytes_read = recv(sock, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';  // Null-terminieren

        // Ausgabe der kompletten Antwort für Debugging
        printf("Client: Antwort erhalten:\n%s\n", buffer);

        // Überprüfe, ob die Antwort den HTTP-Status "200 OK" enthält
        if (strstr(buffer, "200 OK")) {
            printf(GREEN "Client: Erfolgreiche Antwort für %s\n" RESET, path);

            // Mutex verwenden, um den Zähler für erfolgreiche Anfragen zu erhöhen
            pthread_mutex_lock(&mutex);
            total_successful++;
            pthread_mutex_unlock(&mutex);

            close(sock);
            return 0;  // Erfolgreiche Anfrage
        } else {
            printf(RED "Client: Fehlerhafte Antwort vom Server für %s: \n%s\n" RESET, path, buffer);
            
            // Mutex verwenden, um den Zähler für fehlgeschlagene Anfragen zu erhöhen
            pthread_mutex_lock(&mutex);
            total_failed++;
            pthread_mutex_unlock(&mutex);

            close(sock);
            return 1;  // Fehlgeschlagene Anfrage
        }
    } else {
        printf(RED "Client: Keine Antwort vom Server für %s erhalten\n" RESET, path);
        close(sock);
        return 1;  // Fehler beim Empfangen
    }
}

void *client_thread(void *arg) {
    int num_requests = *(int *)arg;
    const char *paths[] = {"/example.html", "/index.html"};
    int num_paths = sizeof(paths) / sizeof(paths[0]);

    for (int j = 0; j < num_requests; j++) {
        // Abwechselnd GET-Anfragen an "/example.html" und "/index.html" senden
        const char *path = paths[j % num_paths];
        test_get_request(path);
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <num_clients> <num_requests>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int num_clients = atoi(argv[1]);
    int num_requests = atoi(argv[2]);

    pthread_t threads[num_clients];
    pthread_mutex_init(&mutex, NULL);  // Initialisiere den Mutex

    printf("Teste den Server mit %d Clients, jeder sendet %d GET-Requests...\n", num_clients, num_requests);

    // Erstelle die Threads für die Clients
    for (int i = 0; i < num_clients; i++) {
        pthread_create(&threads[i], NULL, client_thread, &num_requests);
    }

    // Warten auf alle Threads
    for (int i = 0; i < num_clients; i++) {
        pthread_join(threads[i], NULL);
    }

    // Berichte über die Testergebnisse
    printf(CYAN "\n===== Testbericht =====\n" RESET);
    printf(GREEN "Erfolgreiche Anfragen: %d\n" RESET, total_successful);
    printf(RED "Fehlgeschlagene Anfragen: %d\n" RESET, total_failed);
    printf(YELLOW "Gesamtanzahl der Anfragen: %d\n" RESET, num_clients * num_requests);
    printf(CYAN "=======================\n" RESET);

    pthread_mutex_destroy(&mutex);  // Zerstöre den Mutex

    return 0;
}
