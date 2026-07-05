#ifndef KERNEL_KLOG_H
#define KERNEL_KLOG_H

void kprintf(const char *format, ...);
void klog_info(const char *message);
void klog_error(const char *message);

#endif
