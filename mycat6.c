// 添加特性测试宏，必须在包含任何头文件之前
#define _POSIX_C_SOURCE 200809L // 启用 POSIX.1-2008 特性，包括 posix_fadvise

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

long io_blocksize(int fd)
{
    // 1. 获取文件的状态信息
    struct stat file_stat;
    if (fstat(fd, &file_stat) == -1)
    {
        perror("fstat failed");
        return -1;
    }

    // 从文件元数据中获取文件系统的推荐块大小
    long fs_block_size = file_stat.st_blksize;

    // 2. 获取系统的内存页大小
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
    {
        perror("sysconf(_SC_PAGESIZE) failed");
        return -1;
    }

    // 3. 确定基本缓冲区大小 (两者中的较大值)
    long base_size = (fs_block_size > page_size) ? fs_block_size : page_size;

    // 4. 根据任务5实验确定的最佳倍数
    const int optimal_multiplier = 64; // 根据你的测试结果调整这个值
    return base_size * optimal_multiplier;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <file>\n", argv[0]);
        return 1;
    }

    int input_fd = open(argv[1], O_RDONLY);
    if (input_fd == -1)
    {
        perror("Failed to open input file");
        return 1;
    }

    // 使用 posix_fadvise 提示内核我们将顺序读取整个文件
    if (posix_fadvise(input_fd, 0, 0, POSIX_FADV_SEQUENTIAL) != 0)
    {
        perror("posix_fadvise failed");
        // 继续执行，不要因为优化失败而退出程序
    }

    long buf_size = io_blocksize(input_fd);
    char *buffer = malloc(buf_size);
    if (!buffer)
    {
        perror("Failed to allocate buffer");
        close(input_fd);
        return 1;
    }

    ssize_t bytes_read;
    while ((bytes_read = read(input_fd, buffer, buf_size)) > 0)
    {
        ssize_t bytes_written = 0;
        while (bytes_written < bytes_read)
        {
            ssize_t result = write(STDOUT_FILENO,
                                   buffer + bytes_written,
                                   bytes_read - bytes_written);
            if (result < 0)
            {
                perror("Write error");
                free(buffer);
                close(input_fd);
                return 1;
            }
            bytes_written += result;
        }
    }

    if (bytes_read == -1)
    {
        perror("Read error");
        free(buffer);
        close(input_fd);
        return 1;
    }

    free(buffer);
    close(input_fd);
    return 0;
}