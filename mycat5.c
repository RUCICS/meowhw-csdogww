#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>   // For read, write, close, sysconf
#include <fcntl.h>    // For open
#include <errno.h>    // For errno
#include <string.h>   // For strerror
#include <stdint.h>   // For uintptr_t
#include <sys/stat.h> // For fstat

// Forward declaration
void *align_alloc(size_t size);
void align_free(void *ptr);

/**
 * @brief 确定用于I/O操作的最佳块大小。
 *
 * 此函数综合考虑了内存页大小和文件系统的块大小，
 * 并返回两者中的较大值，以确保高效的内存和磁盘I/O。
 *
 * @param fd 要查询的文件的文件描述符。
 * @return long 推荐的缓冲区大小（以字节为单位），失败时返回-1。
 */
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

    // 4. 应用实验确定的最佳倍数 (64倍是基于典型实验结果的估计值，
    //    实际上应该用你的实验脚本确定的结果替换)
    const int optimal_multiplier = 16;
    return base_size * optimal_multiplier;
}

/**
 * @brief 分配一段内存，其起始地址对齐到系统内存页边界。
 *
 * @param size 需要分配的最小字节数。
 * @param page_size 对齐所依据的页大小。
 * @return void* 成功时返回一个页对齐的指针，失败时返回 NULL。
 */
void *align_alloc_generic(size_t size, long page_size)
{
    if (page_size <= 0 || (page_size & (page_size - 1)) != 0)
    {
        // 页大小必须是正的2的整数次幂
        return NULL;
    }
    size_t total_size = size + page_size + sizeof(void *);
    void *original_ptr = malloc(total_size);
    if (original_ptr == NULL)
    {
        return NULL;
    }
    uintptr_t aligned_addr = (uintptr_t)((char *)original_ptr + sizeof(void *) + page_size - 1) & (~((uintptr_t)page_size - 1));
    void *aligned_ptr = (void *)aligned_addr;
    *((void **)aligned_ptr - 1) = original_ptr;
    return aligned_ptr;
}

/**
 * @brief 释放由 align_alloc_generic 分配的内存。
 *
 * @param ptr 由 align_alloc_generic 返回的指针。
 */
void align_free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    void *original_ptr = *((void **)ptr - 1);
    free(original_ptr);
}

/**
 * @brief 将指定文件的内容通过缓冲区写入到标准输出。
 */
int buffered_cat(int fd, void *buffer, long buf_size)
{
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, buf_size)) > 0)
    {
        ssize_t total_bytes_written = 0;
        while (total_bytes_written < bytes_read)
        {
            ssize_t bytes_written = write(STDOUT_FILENO, (char *)buffer + total_bytes_written, bytes_read - total_bytes_written);
            if (bytes_written == -1)
            {
                fprintf(stderr, "Error writing to stdout: %s\n", strerror(errno));
                return -1;
            }
            total_bytes_written += bytes_written;
        }
    }

    if (bytes_read == -1)
    {
        fprintf(stderr, "Error reading from file: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "Usage: %s <filename>\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        fprintf(stderr, "Error opening file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    // 在打开文件后，根据文件描述符获取最佳缓冲区大小
    long buf_size = io_blocksize(fd);
    if (buf_size == -1)
    {
        // io_blocksize 内部已打印错误信息
        close(fd);
        return 1;
    }

    // 我们需要一个对齐基准，内存页大小是最好的选择
    long align_size = sysconf(_SC_PAGESIZE);
    if (align_size == -1)
    {
        perror("sysconf(_SC_PAGESIZE) failed");
        close(fd);
        return 1;
    }

    // 使用我们自定义的对齐分配函数
    // 注意：即使 buf_size 是 fs_block_size，我们仍然按 page_size 对齐，
    // 因为内存对齐是关于内存系统的，而不是文件系统的。
    void *buffer = align_alloc_generic(buf_size, align_size);
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate aligned memory.\n");
        close(fd);
        return 1;
    }

    // 打印信息用于验证
    printf("Chosen buffer size: %ld bytes (aligned to %ld bytes)\n", buf_size, align_size);

    if (buffered_cat(fd, buffer, buf_size) == -1)
    {
        align_free(buffer);
        close(fd);
        return 1;
    }

    align_free(buffer);

    if (close(fd) == -1)
    {
        fprintf(stderr, "Error closing file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    return 0;
}
