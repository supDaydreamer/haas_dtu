# main_app 服务化部署（OpenWrt/procd）

目标：让 `main_app/haas_dtu` 脱离 SSH 会话运行，避免 SSH 断开后进程退出、上传停止。

## 1. 安装服务脚本（设备上执行）

```sh
cp /root/main_app/tools/main_app.initd /etc/init.d/main_app
chmod +x /etc/init.d/main_app
```

如果你的工程目录不是 `/root/main_app`，请把 `cp` 源路径替换为实际路径。

## 2. 启用并启动服务

```sh
/etc/init.d/main_app enable
/etc/init.d/main_app restart
```

## 3. 运行状态检查

```sh
pidof main_app
pidof haas_dtu
tail -n 50 /tmp/main_app.log
```

## 4. 验证是否已脱离 SSH 终端

将下面 `PID` 替换为实际进程号：

```sh
PID=<pid>
ls -l /proc/$PID/fd/0
ls -l /proc/$PID/fd/1
```

预期：
- `fd/0` 指向 `/dev/null`
- `fd/1` 不再是 `/dev/pts/*`

## 5. 异常恢复验证

```sh
kill -9 <pid>
sleep 3
pidof main_app
pidof haas_dtu
```

预期：服务会被 `procd` 自动拉起。

