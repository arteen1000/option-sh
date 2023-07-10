This is an option-oriented shell interface, capable of turning entire shell scripts into a single command.

Options are interpreted and executed sequentially, from left to write. Here is a comprehensive list of options:

```
--append
--cloexec
--creat
--directory
--dsync
--excl
--nofollow
--nonblock
--rsync
--sync
--trunc
```
The following are file-opening flags. They are applied to the next invocation of
```
--rdonly file
--wronly file
--rdwr file
```
which opens the file in that designated mode. Each of the above consumes one "file number", starting at 0.

```
--pipe
```
consumes 2 file numbers, the first being for the read-end and the second for the write-end. A pipe is automatically created with `FD_CLOEXEC`.

```
--command i o e cmd ...
```
There are 4 mandatory arguments. The first 3 specify where to direct input, output, and error. `i`, `o`, `e` specify the standard; otherwise, a file number may be used. `cmd` is a command name followed by 0 or more options, none of which may begin with `--` as that signals the termination of the options to the command.

```
--wait
```
signals `osh` to wait for all children, emitting status or signal information to output.

```
--chdir path
```
signals `osh` to change its working directory to `path`, affecting interpretation of all subsequent options.

```
--close file_number
```
closes the file descriptor corresponding to `file_number` in the `osh` process.

The following script:
```
../osh \
  --chdir data \
  --rdonly a \
  --pipe \
  --pipe \
  --creat --trunc --wronly c \
  --creat --append --wronly d \
  --command 3 5 6 tr A-Z a-z \
  --command 0 2 6 sort \
  --command 1 4 6 cat b - \
  --close 1 \
  --close 2 \
  --close 3 \
  --close 4 \
  --wait
```

does the following:
```
--chdir data // change the working directory to data
--rdonly a // open a as read-only, file number 0
--pipe // a pipe, file number 1 -> read-end, file number 2 -> write-end
--pipe // a pipe, file number 3 -> read-end, file number 4 -> write-end
--creat --trunc --wronly c // open c as write-only with O_CREAT, O_TRUNC flags, file number 5
--creat --apend --wronly d // open d as write-only with O_CREAT, O_APPEND flags, file number 6
--command 3 5 6 tr A-Z a-z // run 'tr A-Z a-z' with input from file number 3, output to 5, error to 6
--command 0 2 6 sort // run 'sort' with input from file number 0, output to 2, error to 6
--command 1 4 6 cat b - // run 'cat b -' with input from file number 1, output to 2, error to 6
--close x // closes the corresponding file number 'x', corresponding to the write and read-ends of the pipes
--wait // wait for all processes and output diagnostic
```

You can see the result of this by doing the following:
```
$ make
$ scripts/osh-script.sh
../osh: exit 0 sort
../osh: exit 0 cat b -
../osh: exit 0 tr A-Z a-z
```

The above script is equivalent to:
```
cd data
(sort < a | cat b - | tr A-Z a-z > c) 2>> d
```
in a more standard shell.

The inspiration for this project was taken from [here](https://web.cs.ucla.edu/classes/fall19/cs111/assign/lab1.html), an old Operating Systems assignment at UCLA, discontinued due to students complaining that it was too difficult.
