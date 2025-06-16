#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // 检查命令行参数
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    // 打开文件
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    char buffer;
    ssize_t bytes_read;
    ssize_t bytes_written;

    // 逐字符读取并输出
    while ((bytes_read = read(fd, &buffer, 1)) > 0)
    {
        bytes_written = write(STDOUT_FILENO, &buffer, 1);
        if (bytes_written == -1)
        {
            fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
            close(fd);
            return 1;
        }
        // 检查是否写入了完整的字符
        if (bytes_written != 1)
        {
            fprintf(stderr, "Warning: incomplete write to stdout\n");
        }
    }

    // 检查读取是否出错
    if (bytes_read == -1)
    {
        fprintf(stderr, "Error reading from file '%s': %s\n", argv[1], strerror(errno));
        close(fd);
        return 1;
    }

    // 关闭文件
    if (close(fd) == -1)
    {
        fprintf(stderr, "Error closing file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    return 0;
}