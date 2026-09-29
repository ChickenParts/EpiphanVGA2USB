# Epiphan 3.33.0.17 binary versus Linux 7.2.5-1-default

This audit used only local files and read-only SSH. No USB access, resets, firmware uploads, module loading, target writes, or reboot occurred.

The original vendor object is symbolized x86-64 ELF with no DWARF. The source wrapper is not an ABI boundary: the binary itself accesses kernel structures and imports kernel APIs directly. Recompiling the wrapper alone is insufficient.

## Directly demonstrated layout failures

| Operation in vendor object | Running layout | Consequence |
| --- | --- | --- |
| `vga2usb_probe` at 0x482ae reads interface+0x70 as the parent device | usb_interface.dev is 0x50; device.parent is 0x40, so parent is interface+0x90 | interface+0x70 is dev.kobj.kset, not dev.parent; subsequent dereferences operate on the wrong object |
| `vga2usb_probe` at 0x48334 subtracts 0xa0 from its parent device pointer to obtain usb_device | usb_device.dev is 0xb0 | Wrong container pointer even if the preceding parent access is repaired |
| `os_usb_read_submit` at 0x42f63 writes transfer_buffer_length at urb+0x80 | transfer_buffer_length is 0x88; +0x80 is num_mapped_sgs | Writes another field while leaving the intended length wrong |
| `os_usb_read_submit` at 0x42f69 stores its completion callback at urb+0xb0 | complete is 0xb8; +0xb0 is context | Kernel callback pointer is not initialized correctly |
| `os_usb_read_submit` at 0x42f74 stores its context at urb+0xa8 | context is 0xb0; +0xa8 is interval/error_count | Overwrites unrelated integer fields |
| `os_dma_user` at 0x7049 reads current_task+0x810 as mm and at 0x7051 takes a semaphore at mm+0x78 | task_struct.mm is 0xab8; mm_struct.mmap_lock is 0x1d0 | Invalid pointer and invalid lock address |
| `vgrabber_v4l2_register` embeds video_device at allocation+0x18 and writes fops at allocation+0x1c0 | video_device.fops is 0xa8, so matching embedded address would be allocation+0xc0 | The kernel receives an incompatible video_device |
| That function writes device_caps at allocation+0x1c8 | current relative offset is 0xb0, requiring allocation+0xc8 | Capabilities are stored in the wrong field |
| That function writes v4l2_dev at allocation+0x4b0 | current relative offset is 0x3d8, requiring allocation+0x3f0 | Registration receives the wrong parent V4L2 pointer |

Current offsets above come from the running target's `/sys/kernel/btf/vmlinux` and `/sys/kernel/btf/videodev`. `pahole` emits warnings about declaration tags but produces the listed structures; retain the raw BTF as the authoritative artifact.

## Missing imports

The running target's `symvers.gz` has no exports for the following original binary imports (wrapper-owned imports excluded):

```
__class_create __mutex_init __put_devmap_managed_page __put_page
_cond_resched devmap_managed_key dma_direct_map_page dma_direct_map_sg
dma_direct_unmap_page dma_direct_unmap_sg ex_handler_refcount fortify_panic
kmem_cache_alloc_trace printk sg_next snd_pcm_limit_hw_rates strlcpy
strncpy vmalloc
videobuf_dqbuf videobuf_iolock videobuf_mmap_free videobuf_mmap_mapper
videobuf_poll_stream videobuf_qbuf videobuf_querybuf
videobuf_queue_vmalloc_init videobuf_read_stream videobuf_reqbufs
videobuf_stop videobuf_streamoff videobuf_streamon videobuf_to_vmalloc
videobuf_vmalloc_free
```

Some of these APIs have straightforward modern alternatives. Legacy videobuf is absent entirely from the current kernel headers and export set; current headers provide videobuf2. Its queue and callback behavior need an explicit implementation or migration. Linking renamed substitutes does not fix the demonstrated structure failures.

`get_user_pages` still receives the appropriate first four register arguments; the binary's fifth null argument is ignored by the current four-argument API. Its surrounding task/mm pointer accesses are unsafe, independently of call arity.

`usb_get_status` is not a demonstrated call-arity failure: at 0x42d2f the binary provides all five arguments, including status type zero and the buffer in r8. Do not list it as a blocker merely because older upstream kernels had a different signature.

`kmalloc_caches` is still exported, but the object directly indexes it and passes old GFP bits into `kmem_cache_alloc_trace`; reconstruct these allocation semantics rather than assuming export presence proves compatibility.

The community 3.33.0.15 `drvwrapper_fix.c:43` supplies an `ex_handler_refcount` that simply returns true. That does not emulate fault recovery or saturating-refcount semantics and should not be copied as a compatibility implementation.

## Native path

The fastest defensible experiment is a small fresh driver for the original 5555:1110 hardware, preferably libusb in user space first, using the symbolized vendor object's device-specific code as a protocol/algorithm reference. Add a V4L2 interface after capture works. This avoids translating unrelated ALSA/PCI support and an obsolete full kernel ABI.

If preserving the binary is required, use a real compatibility layer with legacy shadow structures and explicit conversions at every kernel boundary, including USB interface/device, URBs and completions, file operations, video_device/ioctl/control tables, task/mm/page/DMA accesses, allocator flags, synchronization objects, and removed videobuf behavior. This is substantially larger than wrapper maintenance. Disabling V4L2/ALSA does not eliminate the mandatory USB probe and transfer layout failures.

Before any load, require an offline oracle that inventories every binary structure read/write and every imported function/data symbol against the exact vendor build ABI and the exact target build ABI. The running target BTF supplies half of that oracle; obtain original 5.4.0-91 configuration and matching debug information/headers or reconstruct its layouts from the object. All old-layout objects passed into the kernel must be intercepted and translated, and all callbacks must translate back. Loader success or a clean modpost is not an ABI proof.

For actual hardware behavior, require a known-good original driver trace or precise reference protocol for 5555:1110: USB descriptors, control request sequence, firmware bytes and destinations, bulk transfers, and frame decoding with known VGA patterns. Validate a new implementation on that reference before permitting firmware/control traffic or loading a kernel compatibility layer on the test host.

## Evidence files

- `blob-disassembly.txt`: complete `objdump -drwC` output.
- `docs/evidence/kernel-7.2.5/*.layout`: selected layouts from the running kernel's BTF.
- Local matching source: `/usr/src/linux-7.2.5-1/include/linux/usb.h`, `include/linux/mm.h`, and `include/media/v4l2-dev.h`.
