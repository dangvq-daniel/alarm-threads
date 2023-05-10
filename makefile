CC=gcc

New_Alarm_Cond: New_Alarm_Cond.c errors.h New_Alarm_Cond.o
	$(CC) -o New_Alarm_Cond New_Alarm_Cond.o -D_POSIX_PTHREAD_SEMANTICS -lpthread
