myApp:	myApp.o fileManager.o disk.o
	gcc -o myApp myApp.o fileManager.o disk.o -lm
	rm -rf *.o *.h.gch *~

myApp.o: myApp.c fileManager.h
	gcc -c myApp.c

fileManager.o:	fileManager.c fileManager.h disk.h
	gcc -c fileManager.c -lm

disk.o:	disk.c disk.h
	gcc -c disk.c
