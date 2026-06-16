cd busybox_shell
source ~/.ghcup/env
export PATH="$HOME/.local/bin:$PATH"
make


#linearized Tree

cd busybox_shell/bnfc
printf "echo hello | wc\n" | ./TestInput
printf 'x=5 ; echo $x\n' | ./TestInput
printf "if echo cond then echo yes fi\n" | ./TestInput
printf "pwd ; echo done\n" | ./TestInput

#how to build the Test input
cd busybox_shell
make -C bnfc build

#RUN OLLLAMA
IF OLLAMA WAS closed, start it from applications.