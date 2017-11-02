# lssdp
light weight SSDP library

### What is SSDP

The Simple Service Discovery Protocol (SSDP) is a network protocol based on the Internet Protocol Suite for advertisement and discovery of network services and presence information.

====

### Support Platform

* Linux Ubuntu
* MAC OSX
* Android
* iOS

====

### How To Build and Test

```
make clean
make

cd test
./daemon.exe
```

====

#### lssdp_ctx:

lssdp context

**port** - SSDP UDP port, 1900 port is general.

**sock** - SSDP socket, created by `lssdp_socket_create`, and close by `lssdp_socket_close`

**neighbor_list** - neighbor list, when received *NOTIFY* or *RESPONSE* packet, neighbor list will be updated.

**neighbor_timeout** - this value will be used by `lssdp_neighbor_check_timeout`. If neighbor is timeout, then remove from neighbor list.

**debug** - SSDP debug mode, show debug message.

**interface** - Network Interface list. Call `lssdp_network_interface_update` to update the list.

**interface_num** - the number of Network Interface list.

**header.search_target** - SSDP Search Target (ST). A potential search target.

**header.unique_service_name** - SSDP Unique Service Name (USN). A composite identifier for the advertisement.

**header.location = prefix + domain + suffix** - [http://] + IP + [:PORT/URI]

**header.sm_id** - Optional field.

**header.device_type** - Optional field.

**network_interface_changed_callback** - when interface is changed, this callback would be invoked.

**neighbor_list_changed_callback** - when neighbor list is changed, this callback would be invoked.

**packet_received_callback** - when received any SSDP packet, this callback would be invoked. It callback is usally used for debugging.

====

#### Function API (8)

##### 01. lssdp_network_interface_update

update network interface.

```
- lssdp.interface, lssdp.interface_num will be updated.
```


##### 02. lssdp_socket_create

create SSDP socket.

```
- SSDP port must be setup ready before call this function. (lssdp.port > 0)

- if SSDP socket is already exist (lssdp.sock > 0), the socket will be closed, and create a new one.

- SSDP neighbor list will be force clean up.
```

##### 03. lssdp_socket_close

close SSDP socket.

```
- if SSDP socket <= 0, will be ignore, and lssdp.sock will be set -1.
- SSDP neighbor list will be force clean up.
```

##### 04. lssdp_socket_read

read SSDP socket.

```
1. if read success, packet_received_callback will be invoked.

2. if received SSDP packet is match to Search Target (lssdp.header.search_target),
   - M-SEARCH: send RESPONSE back
   - NOTIFY/RESPONSE: add/update to SSDP neighbor list
```

```
- SSDP socket and port must be setup ready before call this function. (sock, port > 0)
- if SSDP neighbor list has been changed, neighbor_list_changed_callback will be invoked.
```

##### 05. lssdp_send_msearch

send SSDP M-SEARCH packet to multicast address (239.255.255.250)

```
- SSDP port must be setup ready before call this function. (lssdp.port > 0)
```

##### 06. lssdp_send_notify

send SSDP NOTIFY packet to multicast address (239.255.255.250)

```
- SSDP port must be setup ready before call this function. (lssdp.port > 0)
```

##### 07. lssdp_neighbor_check_timeout

check neighbor in list is timeout or not. (lssdp.neighbor_timeout)

the timeout neighbor will be remove from the list.

```
- if neighbor be removed, neighbor_list_changed_callback will be invoked.
```

##### 08. lssdp_set_log_callback

setup SSDP log callback. All SSDP library log will be forward to here.
