#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // For read, write, close, sysconf
#include <fcntl.h>  // For open
#include <errno.h>  // For errno
#include <string.h> // For strerror
#include <stdint.h> // For uintptr_t

// Forward declaration for io_blocksize
long io_blocksize(void);

/**
 * @brief 分配一段内存，其起始地址对齐到系统内存页边界。
 *
 * @param size 需要分配的最小字节数。
 * @return void* 成功时返回一个页对齐的指针，失败时返回 NULL。
 */
void *align_alloc(size_t size)
{
    long page_size = io_blocksize();
    if (page_size == -1)
    {
        return NULL; // io_blocksize failed
    }

    // 1. 分配额外空间：需要 size 字节给用户，需要空间来保证对齐，
    //    还需要空间 (sizeof(void*)) 来存储原始指针。
    size_t total_size = size + page_size + sizeof(void *);
    void *original_ptr = malloc(total_size);
    if (original_ptr == NULL)
    {
        return NULL; // malloc failed
    }

    // 2. 寻找对齐地址。
    //    (char*)original_ptr + sizeof(void*) 是我们可以开始存放用户数据的最低地址。
    //    通过加上 page_size - 1 并进行位掩码操作，可以找到下一个对齐点。
    uintptr_t aligned_addr = (uintptr_t)((char *)original_ptr + sizeof(void *) + page_size - 1) & (~((uintptr_t)page_size - 1));
    void *aligned_ptr = (void *)aligned_addr;

    // 3. "藏匿"原始指针。
    //    在对齐指针的前面，存下原始的 malloc 指针。
    *((void **)aligned_ptr - 1) = original_ptr;

    // 4. 返回对齐指针给用户。
    return aligned_ptr;
}

/**
 * @brief 释放由 align_alloc 分配的内存。
 *
 * @param ptr 由 align_alloc 返回的指针。
 */
void align_free(void *ptr)
{
    if (ptr == NULL)
    {
        return;
    }
    // 1. "寻找"原始指针。
    //    通过将对齐指针向前移动一个位置，我们找到了之前存储的原始指针。
    void *original_ptr = *((void **)ptr - 1);

    // 2. 正确释放。
    free(original_ptr);
}

/**
 * @brief 确定用于I/O操作的最佳块大小 (内存页大小)。
 */
long io_blocksize(void)
{
    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size == -1)
    {
        perror("sysconf(_SC_PAGESIZE) failed");
        return 4096; // Fallback
    }
    return page_size;
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

    long buf_size = io_blocksize();

    // 使用我们自定义的对齐分配函数
    void *buffer = align_alloc(buf_size);
    if (buffer == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate aligned memory.\n");
        close(fd);
        return 1;
    }
    // 打印信息用于验证
    // printf("Buffer size: %ld, Aligned buffer address: %p\n", buf_size, buffer);

    if (buffered_cat(fd, buffer, buf_size) == -1)
    {
        align_free(buffer); // 确保在出错时也释放内存
        close(fd);
        return 1;
    }

    // 使用我们自定义的释放函数
    align_free(buffer);

    if (close(fd) == -1)
    {
        fprintf(stderr, "Error closing file '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    return 0;
}
