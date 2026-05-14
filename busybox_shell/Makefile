CC = gcc
CFLAGS = -Wall -Wextra -std=c11

TARGET = busybox_shell

SRC = main.c \
      cmd_localdate.c \
      cmd_ls.c \
      cmd_cat.c \
      cmd_pkg.c \
      cmd_pwd.c \
      cmd_wc.c \
      cmd_touch.c \
      cmd_mkdir.c \
      cmd_rmdir.c \
      cmd_echo.c \
      cmd_whoami.c \
      cmd_clear.c \
      cmd_id.c \
      cmd_uname.c \
      cmd_head.c \
      cmd_tail.c \
      cmd_cp.c \
      cmd_mv.c \
      cmd_rm.c \
      cmd_dirname.c \
      cmd_du.c \
      registry.c \
      register_all_commands.c \
      json_utils.c \
      $(wildcard argtable3/src/*.c)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) myshell localdate
	find . -name '*.o' -delete

run: all
	./$(TARGET)

help: all
	./$(TARGET) help

test: all
	./$(TARGET) localdate
	./$(TARGET) localdate -h
	./$(TARGET) ls
	./$(TARGET) ls -a
	./$(TARGET) cat Makefile
	./$(TARGET) wc Makefile
	./$(TARGET) pkg
	./$(TARGET) pkg -h
	./$(TARGET) pwd
	./$(TARGET) touch test_touch.tmp
	./$(TARGET) mkdir -p test_dir/subdir
	./$(TARGET) rmdir test_dir/subdir
	./$(TARGET) rmdir test_dir
	./$(TARGET) echo hello world | grep "^hello world$$"
	test "$$(./$(TARGET) echo -n hello)" = "hello"
	./$(TARGET) whoami | grep "$$(whoami)"
	./$(TARGET) id | grep "uid="
	./$(TARGET) uname | grep "$$(uname -s)"
	printf "one\ntwo\nthree\n" > test_lines.tmp
	./$(TARGET) head -n 1 test_lines.tmp | grep "^one$$"
	./$(TARGET) tail -n 1 test_lines.tmp | grep "^three$$"
	./$(TARGET) cp test_lines.tmp test_copy.tmp
	./$(TARGET) mv test_copy.tmp test_move.tmp
	./$(TARGET) dirname a/b/c | grep "^a/b$$"
	./$(TARGET) du test_move.tmp | grep "test_move.tmp"
	./$(TARGET) rm test_lines.tmp test_move.tmp
	./$(TARGET) clear > /dev/null
	rm -f test_touch.tmp
	printf "localdate\npkg\npwd\nwc Makefile\necho hello shell\nwhoami\nid\nuname\nhelp mkdir\nexit\n" | ./$(TARGET)
