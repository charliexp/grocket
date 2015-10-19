目录树
│
├─docs                                     文档目录
│      idea.txt                            一些点子和开始这个项目的初忠
│      tree.txt                            本文件
│      changelog.txt                       change log
│      
└─src                                      源码目录
    ├─include                              整个服务器的对外接口目录
    │      gr_stdinc.h                     基本数据类型头文件，调用方可以不依赖该文件。
    │      grocket.h                       整个服务器的对外接口头文件，写服务器扩展模块用。
    │      libgrocket.h                    如果使用者需要自己提供服务器可执行程序，则该头文件提供支持
    │      
    ├─library                              基础函数库实现。通过src/include对外暴露
    │      library.sln                     Windows版工程文件
    │      
    ├─module                               服务器扩展模块实现。通过对外接口与服务器交互
    │      module.sln                      Windows版工程文件
    │      
    ├─server                               轻量通用服务器
    │  │  build_linux.sh                   Linux 下的编译脚本
    │  │  build_mac.sh                     Mac OS X 和 BSD 下的编译脚本
    │  │  Makefile.linux                   Linux 下的 Makefile
    │  │  Makefile.mac                     Mac OS X 和 BSD 下的 Makefile
    │  │  server.sln                       Windows版工程文件
    │  │          
    │  ├─conf                             配置文件目录
    │  │      grocketd.ini                 服务器配置文件示例
    │  │          
    │  ├─demo_module                      基于服务器框架的扩展模块示例工程
    │  │      demo_module.c                基于服务器框架的扩展模块示例代码
    │  │          
    │  ├─demo_server                      基于服务器框架的独立可执行程序示例工程
    │  │      demo_server.c                基于服务器框架的独立可执行程序示例代码
    │  │          
    │  ├─include                           轻量通用服务器内部的子模块间接口
    │  │      gr_atomic.h                  [tool]原子加减操作
    │  │      gr_backend.h                 后台维护类线程，比如踢连接啦等
    │  │      gr_config.h                  配置相关
    │  │      gr_conn.h                    连接处理相关
    │  │      gr_dll.h                     动态连接库相关功能
    │  │      gr_errno.h                   错误码
    │  │      gr_event.h                   [tool]事件功能
    │  │      gr_global.h                  整个服务器的根对象。它也是本程序的唯一一个全局变量
    │  │      gr_http.h                    HTTP协议处理模块
    │  │      gr_ini.h                     ini文件操作
    │  │      gr_library_impl.h            服务器功能函数库
    │  │      gr_list.h                    [tool]双链表C接口
    │  │      gr_log.h                     日志输出功能
    │  │      gr_mem.h                     [tool]内存分配与释放功能
    │  │      gr_module.h                  用户模块
    │  │      gr_poll.h                    网络高并发操作系统机制封装
    │  │      gr_server_impl.h             服务器模块
    │  │      gr_socket.h                  [tool]SOCKET工具
    │  │      gr_tcp_accept.h              TCP accept
    │  │      gr_tcp_close.h               TCP 关连接
    │  │      gr_tcp_in.h                  TCP读功能
    │  │      gr_tcp_out.h                 TCP写功能
    │  │      gr_thread.h                  [tool]一些线程相关的工具函数
    │  │      gr_tools.h                   [tool]一些工具函数
    │  │      gr_udp_in.h                  UDP读功能
    │  │      gr_udp_out.h                 UDP写功能
    │  │      gr_worker.h                  工作线程或进程
    │  │  
    │  ├─grocketd                          轻量通用服务器可执行版本。
    │  │      grocketd.c                   轻量通用服务器主程序。它依赖libgrocket。
    │  │      grocketd.vcxproj             Windows版项目文件
    │  │      
    │  └─libgrocket                        轻量通用服务器静态函数库版本
    │          gr_backend.c                后台维护类线程，比如踢连接啦等
    │          gr_config.c                 配置相关
    │          gr_conn.c                   连接处理相关
    │          gr_dll.c                    动态连接库相关功能
    │          gr_event.c                  [tool]事件功能
    │          gr_http.c                   HTTP协议处理
    │          gr_http_default.c           HTTP默认命令实现
    │          gr_ini.c                    ini文件操作
    │          gr_library_impl.c           服务器功能函数库
    │          gr_log.c                    日志输出功能
    │          gr_module.c                 用户模块
    │          gr_poll_bsd.c               BSD和Mac OS X网络高并发操作系统机制封装，KQueue
    │          gr_poll_linux.c             Linux网络高并发操作系统机制封装，EPOLL
    │          gr_poll_windows.c           Windows网络高并发操作系统机制封装，IOCP
    │          gr_server_impl.c            服务器模块
    │          gr_server_impl_posix.c      posix操作系统下服务器模块平台特定功能
    │          gr_server_impl_windows.c    Windows版服务器模块平台特定功能
    │          gr_socket.c                 [tool]SOCKET工具
    │          gr_tcp_accept.c              TCP accept
    │          gr_tcp_close.c              TCP 关连接
    │          gr_tcp_in.c                  TCP读功能
    │          gr_tcp_out.c                 TCP写功能
    │          gr_thread.c                 一些线程相关工具函数
    │          gr_tools.c                  [tool]一些工具函数
    │          gr_tools_mac.m              [tool]Mac OS X系统下的一些工具函数，有 Objective-C 代码
    │          gr_udp_in.c                  UDP读功能
    │          gr_udp_out.c                 UDP写功能
    │          gr_worker.c                 工作线程或进程组
    │          libgrocket.c                如果使用者需要自己提供服务器可执行程序，则该文件提供支持函数
    │          libgrocket.vcxproj          Windows版项目文件
    │          server_object.h             服务器内置的一个函数库对象，供服务器扩展程序调用服务器内置功能的
    │          server_object.c             服务器内置的一个函数库对象，供服务器扩展程序调用服务器内置功能的
    │          tcp_io.h                    收发相关的私有逻辑。被Windows破坏的私有代码。
    │          
    └─third                              第三方源码库
           gperftools-2.1.tar.gz           google的内存分配器

