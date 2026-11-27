#define MSG_LEN 1024
#define SERV_PORT "8080"
#define SERV_ADDR "127.0.0.1"
#define PROTO_MAX_PAYLOAD 65536

// Copie src dans dst en tronquant si besoin, et termine dst avec '\0'
static inline void copy_str(char *dst, size_t size, const char *src)
{
	snprintf(dst, size, "%s", src);
}