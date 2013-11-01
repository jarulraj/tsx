
// DEBUG MACRO
#ifdef DEBUG
#define debug(M, ...) fprintf(stderr, "[%s,%d] : " M, __FILE__, __LINE__, ##__VA_ARGS__)
#else
#define debug(M, ...)
#endif
 
