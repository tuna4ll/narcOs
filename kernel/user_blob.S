.section .rodata.userblob, "a"
.global user_blob_start
.global user_blob_end
user_blob_start:
.incbin "build/userland/app"
user_blob_end:
