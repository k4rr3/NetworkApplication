#!/usr/bin/env python3.8
import socket, sys
from enum import Enum


class Status(Enum):
    DISCONNECTED = 0xA0
    WAIT_REG_RESPONSE = 0xA2
    WAIT_DB_CHECK = 0xA4
    REGISTERED = 0xA6
    SEND_ALIVE = 0xA8


class PduRegister(Enum):
    REGISTER_REQ = 0x00
    REGISTER_ACK = 0x02
    REGISTER_NACK = 0x04
    REGISTER_REJ = 0x06
    ERROR = 0x0F


class PduAlive(Enum):
    ALIVE_INF = 0x10
    ALIVE_ACK = 0x12
    ALIVE_NACK = 0x14
    ALIVE_REJ = 0x16


class PduSendCfg(Enum):
    SEND_FILE = 0x20
    SEND_DATA = 0x22
    SEND_ACK = 0x24
    SEND_NACK = 0x26
    SEND_REJ = 0x28
    SEND_END = 0x2A


class PduGetCfg(Enum):
    GET_FILE = 0x30
    GET_DATA = 0x32
    GET_ACK = 0x34
    GET_NACK = 0x36
    GET_REJ = 0x38
    GET_END = 0x3A


status_names = {
    Status.DISCONNECTED: "DISCONNECTED",
    Status.WAIT_REG_RESPONSE: "WAIT_REG_RESPONSE",
    Status.WAIT_DB_CHECK: "WAIT_DB_CHECK",
    Status.REGISTERED: "REGISTERED",
    Status.SEND_ALIVE: "SEND_ALIVE"
}

pdu_types = {
    PduRegister.REGISTER_REQ: "REGISTER_REQ",
    PduRegister.REGISTER_ACK: "REGISTER_ACK",
    PduRegister.REGISTER_NACK: "REGISTER_NACK",
    PduRegister.REGISTER_REJ: "REGISTER_REJ",
    PduRegister.ERROR: "ERROR",
    PduAlive.ALIVE_INF: "ALIVE_INF",
    PduAlive.ALIVE_ACK: "ALIVE_ACK",
    PduAlive.ALIVE_NACK: "ALIVE_NACK",
    PduAlive.ALIVE_REJ: "ALIVE_REJ",
    PduSendCfg.SEND_FILE: "SEND_FILE",
    PduSendCfg.SEND_DATA: "SEND_DATA",
    PduSendCfg.SEND_ACK: "SEND_ACK",
    PduSendCfg.SEND_NACK: "SEND_NACK",
    PduSendCfg.SEND_REJ: "SEND_REJ",
    PduSendCfg.SEND_END: "SEND_END",
    PduGetCfg.GET_FILE: "GET_FILE",
    PduGetCfg.GET_DATA: "GET_DATA",
    PduGetCfg.GET_ACK: "GET_ACK",
    PduGetCfg.GET_NACK: "GET_NACK",
    PduGetCfg.GET_REJ: "GET_REJ",
    PduGetCfg.GET_END: "GET_END"
}


class Pdu:
    def __init__(self, pdu_type, system_id, mac_address, alea, data):
        self.pdu_type = pdu_type
        self.system_id = system_id
        self.mac_address = mac_address
        self.alea = alea
        self.data = data

    def convert_pdu_to_pkg(self, protocol_data_size):
        pdu = [chr(0)] * protocol_data_size
        pdu[0] = chr(self.pdu_type)
        pdu[1: 1 + len(self.system_id)] = self.system_id
        pdu[8:8 + len(self.mac_address)] = self.mac_address
        pdu[21:21 + len(self.alea)] = self.alea
        pdu[28:28 + len(self.data)] = self.data
        pdu = ''.join(pdu).encode()
        return pdu


def convert_pkg_to_pdu(package):
    system_id = str(package[1:7].decode()).replace('\x00', '')
    mac_address = package[8:20].decode().replace('\x00', '')
    alea = package[21:28].decode().replace('\x00', '')
    data = package[29:]
    return Pdu(package[0], system_id, mac_address, alea, data)


class Cfg:
    def __init__(self, id, mac, udp_port, tcp_port):
        self.id = id
        self.mac = mac
        self.udp_port = udp_port
        self.tcp_port = tcp_port


class KnownDevice:
    def __init__(self, id, mac, status, alea, ip_address):
        self.id = id
        self.mac = mac
        self.status = status
        self.alea = alea
        self.ip_address = ip_address


known_devices = []
UDP_PKG_SIZE = 78
TCP_PKG_SIZE = 178


def main():
    read_server_cfg()
    read_known_devices()
    attend_reg_requests()


def read_server_cfg():
    global server_cfg
    cfg = []
    server_cfg_file = open("server.cfg", "r")
    line = " "  # readline() returns an empty string, the end of the file has been reached,
    # while a blank line is represented by '\n', a string containing only a single newline.
    while line != '':
        line = server_cfg_file.readline()
        if line != '\n' and line != '':
            cfg.append(line.split())
            # server id   mac        udp_port    tcp_port
    server_cfg = Cfg(cfg[0][1], cfg[1][1], cfg[2][1], cfg[3][1])
    print(server_cfg.id, " ", server_cfg.mac, " ", server_cfg.udp_port, " ", server_cfg.tcp_port, "\n")


def read_known_devices():
    global known_devices
    equips_dat_file = open("equips.dat", "r")
    cfg = []
    line = " "
    while line != '':
        line = equips_dat_file.readline()
        if line != '\n' and line != '':
            cfg.append(line.split())
            known_devices.append(KnownDevice(cfg[0][0], cfg[0][1], None, None, None))
            cfg.pop()
    for i in range(0, len(known_devices)):
        print(known_devices[i].id, " ", known_devices[i].mac)


def attend_reg_requests():
    global UDP_PKG_SIZE
    sockfd_udp = create_udp_socket()
    while True:
        data, addr = sockfd_udp.recvfrom(UDP_PKG_SIZE)
        print(data)
        received_pdu = convert_pkg_to_pdu(data)
        if is_autorized_client(received_pdu.system_id, received_pdu.mac_address):
            print("Cliente autorizado")
            pdu_udp = Pdu(PduRegister.REGISTER_ACK.value, known_devices[0].id, known_devices[0].mac, "123456",
                          '').convert_pdu_to_pkg(UDP_PKG_SIZE)
            sockfd_udp.sendto(pdu_udp, addr)
            print("hola")


def is_autorized_client(id, mac):
    global known_devices
    for i in range(len(known_devices)):
        if known_devices[i].id == id and known_devices[i].mac == mac:
            return True
    return False


def create_udp_socket():
    global server_cfg
    server_ip = "127.0.0.1"
    sockfd_udp = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    sockfd_udp.bind((server_ip, int(server_cfg.udp_port)))
    return sockfd_udp


def create_tcp_socket():
    global server_cfg
    server_ip = "127.0.0.1"
    sockfd_tcp = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
    sockfd_tcp.bind((server_ip, int(server_cfg.tcp_port)))
    return sockfd_tcp


if __name__ == '__main__':
    # try:
    main()
# except KeyboardInterrupt as e:  # Ctrl-C
#     raise e
# except SystemExit as e:  # sys.exit()
#     raise e
# except Exception as e:
#     print(f'ERROR, UNEXPECTED EXCEPTION')
#     print(f'{str(e)}')
#     sys.exit(-1)
