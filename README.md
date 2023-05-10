# alarm-threads

Using Posix-threads and semaphores to create an alarm that can store and count down multiples alarms at the same time while printing designated messages.

1. First copy the files "New_Alarm_Cond.c", "makefile" and "errors.h" into your
   own directory.

2. To compile the program "alarm_cond.c", use the following command:

   make

3. Type "./New_Alarm_Cond" to run the executable code.

4. At the prompt "ALARM>", there are two possible input types. 
   The first one start a new alarm, and the second one update
   an existing alarm

   Alarm> Start_Alarm(10): Group(25) 6 Tell Bill to pick up Ted at 5pm
   Alarm> Start_Alarm(100): Group(25) 6 Tell Bill to pick up Ted at 5pm
   Alarm> Change_Alarm(10): Group(25) 100 Pick up Ted later at 9pm

  (To exit from the program, type Ctrl-d.)
