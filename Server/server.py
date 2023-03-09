#!/usr/bin/env python3.8
import random
import socket
import sys
import threading
from datetime import datetime
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
    Status.DISCONNECTED.value: "DISCONNECTED",
    Status.WAIT_REG_RESPONSE.value: "WAIT_REG_RESPONSE",
    Status.WAIT_DB_CHECK.value: "WAIT_DB_CHECK",
    Status.REGISTERED.value: "REGISTERED",
    Status.SEND_ALIVE.value: "SEND_ALIVE"
}

pdu_types = {
    PduRegister.REGISTER_REQ.value: "REGISTER_REQ",
    PduRegister.REGISTER_ACK.value: "REGISTER_ACK",
    PduRegister.REGISTER_NACK.value: "REGISTER_NACK",
    PduRegister.REGISTER_REJ.value: "REGISTER_REJ",
    PduRegister.ERROR.value: "ERROR",
    PduAlive.ALIVE_INF.value: "ALIVE_INF",
    PduAlive.ALIVE_ACK.value: "ALIVE_ACK",
    PduAlive.ALIVE_NACK.value: "ALIVE_NACK",
    PduAlive.ALIVE_REJ.value: "ALIVE_REJ",
    PduSendCfg.SEND_FILE.value: "SEND_FILE",
    PduSendCfg.SEND_DATA.value: "SEND_DATA",
    PduSendCfg.SEND_ACK.value: "SEND_ACK",
    PduSendCfg.SEND_NACK.value: "SEND_NACK",
    PduSendCfg.SEND_REJ.value: "SEND_REJ",
    PduSendCfg.SEND_END.value: "SEND_END",
    PduGetCfg.GET_FILE.value: "GET_FILE",
    PduGetCfg.GET_DATA.value: "GET_DATA",
    PduGetCfg.GET_ACK.value: "GET_ACK",
    PduGetCfg.GET_NACK.value: "GET_NACK",
    PduGetCfg.GET_REJ.value: "GET_REJ",
    PduGetCfg.GET_END.value: "GET_END"
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
    try:
        data = package[29:].decode().replace('\x00', '')
    except:
        data = ''
    return Pdu(package[0], system_id, mac_address, alea, data)


class Cfg:
    def __init__(self, id, mac, udp_port, tcp_port):
        self.id = id
        self.mac = mac
        self.udp_port = udp_port
        self.tcp_port = tcp_port


class KnownDevice:
    def __init__(self, id, mac, alea, ip_address, status):
        self.id = id
        self.mac = mac
        self.status = status
        self.alea = alea
        self.ip_address = ip_address


devices_file = 'equips.dat'
server_file = 'server.cfg'
debug = 1
server_cfg = None
clients = []
UDP_PKG_SIZE = 78
TCP_PKG_SIZE = 178
sockfd_udp = -1
sockfd_tcp = -1


def main():
    global sockfd_udp, sockfd_tcp
    read_server_cfg()
    read_known_clients()
    read_args_cfg_files()
    sockfd_udp = create_udp_socket()
    sockfd_tcp = create_tcp_socket()
    reg_and_alive()


def print_time(text):
    current_time = datetime.now().strftime('%H:%M:%S')
    print(current_time, text)


def read_args_cfg_files():
    global devices_file, server_file, debug
    for i in range(1, len(sys.argv)):
        if sys.argv[i] == '-d':
            debug = 1
        if sys.argv[i] == '-u':
            devices_file = sys.argv[i + 1]
        if sys.argv[i] == '-c':
            server_file = sys.argv[i + 1]
    if debug == 1:
        print_time('DEBUG =>  Llegits paràmetres arxiu de configuració')


def read_server_cfg():
    global server_cfg, debug, server_file
    cfg = []
    server_cfg_file = open(server_file, "r")
    line = " "  # readline() returns an empty string, the end of the file has been reached,
    # while a blank line is represented by '\n', a string containing only a single newline.
    while line != '':
        line = server_cfg_file.readline()
        if line != '\n' and line != '':
            cfg.append(line.split())
            # server id   mac        udp_port    tcp_port
    server_cfg = Cfg(cfg[0][1], cfg[1][1], cfg[2][1], cfg[3][1])
    # print(server_cfg.id, " ", server_cfg.mac, " ", server_cfg.udp_port, " ", server_cfg.tcp_port, "\n")
    if debug == 1:
        print_time("DEBUG =>  Llegits paràmetres línia de comandes ")


def read_known_clients():
    global clients, devices_file
    equips_dat_file = open(devices_file, "r")
    cfg = []
    line = " "
    while line != '':
        line = equips_dat_file.readline()
        if line != '\n' and line != '':
            cfg.append(line.split())
            clients.append(KnownDevice(cfg[0][0], cfg[0][1], None, None, status_names[Status.DISCONNECTED.value]))
            cfg.pop()
    # for i in range(0, len(clients)):
    #     print(clients[i].id, " ", clients[i].mac)
    print_time("INFO  =>  Llegits 9 equips autoritzats en el sistema")


def reg_and_alive():
    thread_cmnd = threading.Thread(target=read_command_line)
    thread_cmnd.start()
    while True:
        attend_reg_requests()
    # thread_cmnd.join()


def attend_reg_requests():
    global UDP_PKG_SIZE, clients, pdu_types, server_cfg, sockfd_udp
    first_pkg = False

    data, addr = sockfd_udp.recvfrom(UDP_PKG_SIZE)
    received_pdu = convert_pkg_to_pdu(data)
    pdu_type = pdu_types[received_pdu.pdu_type]
    print_time('DEBUG =>  Rebut: bytes=' + str(
        len(data)) + ' , comanda=' + pdu_type + ', id=' + received_pdu.system_id + ', mac=' + received_pdu.mac_address + ', alea=' + received_pdu.alea + ', dades=' + received_pdu.data)
    cli_idx = is_known_client(received_pdu.system_id)
    if cli_idx >= 0:
        clients[cli_idx].status = Status.WAIT_REG_RESPONSE.value
        print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[Status.WAIT_REG_RESPONSE.value])

    if cli_idx == -1:
        pdu = Pdu(PduRegister.REGISTER_REJ.value, '00000000000', '000000000000', '000000',
                  'Motiu rebuig: Equip ' + received_pdu.system_id + ' no autoritzat')
    elif (clients[cli_idx].status == 'REGISTERED' or clients[cli_idx].status == 'ALIVE') and check_client_data(
            cli_idx, received_pdu):
        pdu = Pdu(PduRegister.REGISTER_ACK.value, clients[cli_idx].id,
                  clients[cli_idx].mac, clients[cli_idx].alea,
                  server_cfg.tcp_port)
    elif received_pdu.alea != '000000' and received_pdu.alea != clients[cli_idx].alea:
        pdu = Pdu(PduRegister.REGISTER_NACK.value, '00000000000', '000000000000', '000000',
                  'Motiu rebuig: Equip ' + received_pdu.system_id + ' alea incorrecte')

    elif received_pdu.alea == '000000':
        clients[cli_idx].alea = alea_generator()
        pdu = Pdu(PduRegister.REGISTER_ACK.value, server_cfg.id, server_cfg.mac, clients[cli_idx].alea,
                  server_cfg.tcp_port)
        clients[cli_idx].ip_address = addr[0]
        clients[cli_idx].status = Status.REGISTERED.value
        print_time('INFO  =>  Acceptat registre. Equip: id=' + clients[cli_idx].id + ', ip=' + clients[
            cli_idx].ip_address + ', mac=' + clients[cli_idx].mac + ' alea=' + received_pdu.alea)
        print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[Status.REGISTERED.value])
        if debug == 1:
            print_time('DEBUG =>  Enviat: bytes=' + str(UDP_PKG_SIZE) + ' , comanda=' + pdu_types[
                pdu.pdu_type] + ', id=' + server_cfg.id + ', mac=' + server_cfg.mac + ', alea=' + received_pdu.alea + ', dades=' + pdu.data)
        first_pkg = False
    elif not first_pkg and addr[0] != clients[cli_idx].ip_address:
        pdu = Pdu(PduRegister.REGISTER_NACK.value, '00000000000', '000000000000', '000000',
                  'Motiu rebuig: Equip ' + received_pdu.system_id + 'amb adreça ip incorrecta')
    sockfd_udp.sendto(pdu.convert_pdu_to_pkg(UDP_PKG_SIZE), addr)


def read_command_line():
    global clients
    while True:
        command = input()
        """Visualitza per pantalla una taula amb tots els equips autoritzats en el sistema
            amb les següents dades: Nom, MAC i estat. Per els equips que estan registrats
            també ha de mostrar l’adreça IP des de la que s’ha connectat i el nombre aleatori
            assignat en la fase de registre."""
        ip_addr_alea = ''
        if command == 'list':
            print_time("\tNom \t|\t Mac \t  |\t   Estat \t|\t Adreça IP \t|\t Alea")
            for i in range(len(clients)):
                if clients[i].status == status_names[Status.REGISTERED.value] or clients[i].status == status_names[
                    Status.SEND_ALIVE.value]:
                    ip_addr_alea = clients[i].ip_address + "\t" + clients[i].alea
                print_time("\t" + str(clients[i].id) + "\t" + str(clients[i].mac) + "\t" + str(
                    clients[i].status) + "\t" + ip_addr_alea)
        elif command == 'quit':
            sys.exit()
        else:
            print("Comanda no reconeguda")


def maintenance_control():
    global sockfd_udp, clients

    pass


def is_known_client(id):
    global clients
    for i in range(len(clients)):
        if clients[i].id == id:
            return i
    return -1


def check_client_data(client_index, received_pdu):
    global clients
    return clients[client_index].id == received_pdu.system_id and clients[
        client_index].mac == received_pdu.mac_address and clients[client_index].alea == received_pdu.alea


def alea_generator():
    random_number = ""
    for i in range(6):
        random_number = random_number + str(random.randint(0, 9))
    return random_number


def create_udp_socket():
    global server_cfg, debug
    server_ip = "127.0.0.1"
    sockfd_udp = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    sockfd_udp.bind((server_ip, int(server_cfg.udp_port)))
    if debug == 1:
        print_time('DEBUG =>  Socket UDP actiu')
    return sockfd_udp


def create_tcp_socket():
    global server_cfg, debug
    server_ip = "127.0.0.1"
    sockfd_tcp = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
    sockfd_tcp.bind((server_ip, int(server_cfg.tcp_port)))
    if debug == 1:
        print_time('DEBUG =>  Socket TCP actiu')
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
