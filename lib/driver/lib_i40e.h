#ifndef _I40E_H__
#define _I40E_H__

#define msleep(t, n)		(t)->tv_sec = 0; \
				(t)->tv_nsec = ((n) * 1000000); \
				nanosleep((t), NULL);
#define usleep(t, n)		(t)->tv_sec = 0; \
				(t)->tv_nsec = ((n) * 1000); \
				nanosleep((t), NULL);

#endif /* _I40E_H__ */
