
# 🧠 Debug Log: Valgrind Memory Error — Invalid Read of Size 1

## 🔍 Issue Summary

While running the proxy server with `valgrind`, the following error was detected:

```
==57442== Invalid read of size 1
==57442==    at 0x10A5E5: parse_http_request (in /home/nazibul/c-program/req-res-parser/server_app)
==57442==    by 0x10975D: main (in /home/nazibul/c-program/req-res-parser/server_app)
==57442==  Address 0x4a8e7c2 is 3 bytes after a block of size 79 alloc'd
==57442==    at 0x4848899: malloc (in /usr/libexec/valgrind/vgpreload_memcheck-amd64-linux.so)
==57442==    by 0x490958E: strdup (strdup.c:42)
==57442==    by 0x10A2BC: parse_http_request (in /home/nazibul/c-program/req-res-parser/server_app)
==57442==    by 0x10975D: main (in /home/nazibul/c-program/req-res-parser/server_app)
==57442== 
```

## 📌 Root Cause

The error occurred due to double pointer increment in the HTTP body parsing section. The **body_start pointer was incremented twice**, causing it to point beyond the allocated buffer:
```
// First increment (CORRECT)
char *body_start = strstr(bufferCopy, "\r\n\r\n");
if (body_start) {
    *body_start = '\0';
    body_start += 4;    // Skip \r\n\r\n - moves to position 79
}

// Second increment (BUG!)
if (body_start) {
    body_start += 4;    // ❌ DUPLICATE! Now at position 83
    if (*body_start != '\0') {
        req->body = strdup(body_start);  // Reading 4 bytes past buffer end
    }
}
```
### Memory Layout:
```
Valid Buffer (79 bytes):
Position: 0  1  2  3  ... 75 76 77 78 | 79 80 81 82 83 84 85 ...
Content:  G  E  T     ... \r \n \r \n | ?  ?  ?  ?  ?  ?  ?  ...
                           ↑           ↑           ↑
                           │           │           │
                    body_start     First +=4   Second +=4
                    (position 75)  (pos 79)    (pos 83)
                                      ↑           ↑
                               1 byte past   4 bytes past
                               valid buffer  valid buffer
```




## 🛠 Tool Used

- [`valgrind`](https://valgrind.org/): a tool for detecting memory leaks, invalid reads/writes, and undefined behavior in C/C++ programs.

Run it using:
```bash
valgrind ./server_app
```

## 📘 Lessons Learned

- Always check **buffer bounds** when accessing adjacent characters.
- Use `valgrind` regularly to catch memory-related bugs.
- Memory overreads can still appear to “work” — but can cause crashes later.
