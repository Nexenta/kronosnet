

typedef enum {IPCHECK_TYPE_ADDRESS, IPCHECK_TYPE_MASK, IPCHECK_TYPE_RANGE} ipcheck_type_t;
typedef	enum {IPCHECK_ACCEPT, IPCHECK_REJECT} ipcheck_acceptreject_t;

int ipcheck_validate(struct sockaddr_storage *checkip);

void ipcheck_clear(void);
int ipcheck_addip(struct sockaddr_storage *ip1, struct sockaddr_storage *ip2,
		  ipcheck_type_t type, ipcheck_acceptreject_t acceptreject);
