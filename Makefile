all: shell

shell: sshell.c
	gcc sshell.c -o shell

clean:
	rm shell