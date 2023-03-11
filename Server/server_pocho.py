#!/usr/bin/env python3.8
import random
import socket
import sys
import threading
from datetime import datetime

DISCONNECTED = 0xA0
WAIT_REG_RESPONSE = 0xA2
WAIT_DB_CHECK = 0xA4
REGISTERED = 0xA6
ALIVE = 0xA8

REGISTER_REQ = 0x00
REGISTER_ACK = 0x02
REGISTER_NACK = 0x04
REGISTER_REJ = 0x06
ERROR = 0x0F

ALIVE_INF = 0x10
ALIVE_ACK = 0x12
ALIVE_NACK = 0x14
ALIVE_REJ = 0x16

SEND_FILE = 0x20
SEND_DATA = 0x22
SEND_ACK = 0x24
SEND_NACK = 0x26
SEND_REJ = 0x28
SEND_END = 0x2A

GET_FILE = 0x30
GET_DATA = 0x32
GET_ACK = 0x34
GET_NACK = 0x36
GET_REJ = 0x38
GET_END = 0x3A

J = 2
R = 2
S = 3
status_names = {
    DISCONNECTED: "DISCONNECTED",
    WAIT_REG_RESPONSE: "WAIT_REG_RESPONSE",
    WAIT_DB_CHECK: "WAIT_DB_CHECK",
    REGISTERED: "REGISTERED",
    ALIVE: "ALIVE"
}

pdu_types = {
    REGISTER_REQ: "REGISTER_REQ",
    REGISTER_ACK: "REGISTER_ACK",
    REGISTER_NACK: "REGISTER_NACK",
    REGISTER_REJ: "REGISTER_REJ",
    ERROR: "ERROR",
    ALIVE_INF: "ALIVE_INF",
    ALIVE_ACK: "ALIVE_ACK",
    ALIVE_NACK: "ALIVE_NACK",
    ALIVE_REJ: "ALIVE_REJ",
    SEND_FILE: "SEND_FILE",
    SEND_DATA: "SEND_DATA",
    SEND_ACK: "SEND_ACK",
    SEND_NACK: "SEND_NACK",
    SEND_REJ: "SEND_REJ",
    SEND_END: "SEND_END",
    GET_FILE: "GET_FILE",
    GET_DATA: "GET_DATA",
    GET_ACK: "GET_ACK",
    GET_NACK: "GET_NACK",
    GET_REJ: "GET_REJ",
    GET_END: "GET_END"
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
    def __init__(self, id, mac, alea, ip_address, status, last_received_pkg):
        self.id = id
        self.mac = mac
        self.status = status
        self.alea = alea
        self.ip_address = ip_address
        self.last_received_pkg = last_received_pkg


devices_file = 'equips.dat'
server_file = 'server.cfg'
debug = 1
server = None
clients = []
UDP_PKG_SIZE = 78
TCP_PKG_SIZE = 178
sock_udp = -1
sock_tcp = -1


def main():
    global sock_udp, sock_tcp
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
    global server, debug, server_file
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
            clients.append(KnownDevice(cfg[0][0], cfg[0][1], None, None, status_names[DISCONNECTED], None))
            cfg.pop()
    # for i in range(0, len(clients)):
    #     print(clients[i].id, " ", clients[i].mac)
    print_time("INFO  =>  Llegits 9 equips autoritzats en el sistema")


def reg_and_alive():
    #thread to wait input in terminal
    thread_cmnd = threading.Thread(target=read_command_line)
    thread_cmnd.start()

    if debug == 1:
        print_time("DEBUG =>  Creat fill per gestionar alives")
    print_time("INFO  =>  Establert temporitzador per control alives")
    while True:
        #main thread
        attend_reg_requests()


def attend_reg_requests():
    global UDP_PKG_SIZE, clients, pdu_types, server, sock_udp
    first_pkg = False
    pdu = None
    try:
        data, addr = sockfd_udp.recvfrom(UDP_PKG_SIZE)
        received_pdu = convert_pkg_to_pdu(data)
        """CHECK IS RECEIVED PDU IS A REGISTER_REQ TYPE"""
        if received_pdu.pdu_type == REGISTER_REQ:
            print_time(
                f"DEBUG => Rebut: bytes={len(data)}, comanda={pdu_types[received_pdu.pdu_type]}, id={received_pdu.system_id}, mac={received_pdu.mac_address}, alea={received_pdu.alea}, dades={received_pdu.data}")
            cli_idx = is_known_client(received_pdu.system_id)
            if cli_idx == -1:
                """ CLIENT NOT AUTORIZED """
                pdu = Pdu(REGISTER_REJ, '00000000000', '000000000000', '000000',
                          'Motiu rebuig: Equip ' + received_pdu.system_id + ' no autoritzat')

            elif (clients[cli_idx].status == 'REGISTERED' or clients[cli_idx].status == 'ALIVE') and check_client_data(
                    cli_idx, received_pdu):
                """CLIENT REGISTERED OR ALIVE"""
                pdu = Pdu(REGISTER_ACK, clients[cli_idx].id, clients[cli_idx].mac, clients[cli_idx].alea,
                          server_cfg.tcp_port)
                print_time(
                    f"INFO  =>  Acceptat registre duplicat. Equip: id={clients[cli_idx].id}, ip={clients[cli_idx].ip_address}, mac={clients[cli_idx].mac} alea={received_pdu.alea}")
                print_time(f"MSG.  =>  Equip Sw-001 passa a estat:{status_names[REGISTERED]}")
                clients[cli_idx].last_received_pkg = datetime.now()
                #print("REGISTERED AT: ",clients[cli_idx].last_received_pkg)
                if debug == 1:
                    print_time(
                        f"DEBUG =>  Enviat: bytes={UDP_PKG_SIZE}, comanda={pdu_types[pdu.pdu_type]}, id={server_cfg.id}, mac={server_cfg.mac}, alea={received_pdu.alea}, dades={pdu.data}")
                # thread to maintain alive phase
                thread_alive = threading.Thread(target=maintenance_control, args=(cli_idx, received_pdu))
                thread_alive.start()
            elif clients[cli_idx].status == "DISCONNECTED":
                """CLIENT DISCONNECTED --> WAIT_REG_RESPONSE"""
                clients[cli_idx].status = status_names[WAIT_REG_RESPONSE]
                print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[WAIT_REG_RESPONSE])

                if received_pdu.alea != '000000' and received_pdu.alea != clients[cli_idx].alea:
                    """BAD ALEA DETECTED"""
                    pdu = Pdu(REGISTER_NACK, '00000000000', '000000000000', '000000',
                              'Motiu rebuig: Equip ' + received_pdu.system_id + ' alea incorrecte')

                elif received_pdu.alea == '000000':
                    """REGISTERING CLIENT..."""
                    clients[cli_idx].alea = alea_generator()
                    pdu = Pdu(REGISTER_ACK, server_cfg.id, server_cfg.mac, clients[cli_idx].alea, server_cfg.tcp_port)
                    clients[cli_idx].ip_address = addr[0]
                    clients[cli_idx].status = status_names[REGISTERED]
                    clients[cli_idx].last_received_pkg = datetime.now()
                   # print("REGISTERED AT: ",clients[cli_idx].last_received_pkg)
                    print_time('INFO  =>  Acceptat registre. Equip: id=' + clients[cli_idx].id + ', ip=' + clients[
                        cli_idx].ip_address + ', mac=' + clients[cli_idx].mac + ' alea=' + received_pdu.alea)
                    print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[REGISTERED])
                    if debug == 1:
                        print_time(
                            f"DEBUG =>  Enviat: bytes={UDP_PKG_SIZE}, comanda={pdu_types[pdu.pdu_type]}, id={server_cfg.id}, mac={server_cfg.mac}, alea={received_pdu.alea}, dades={pdu.data}")
                    first_pkg = False
                    # thread to maintain alive phase
                    thread_alive = threading.Thread(target=maintenance_control, args=(cli_idx, received_pdu))
                    thread_alive.start()
                elif not first_pkg and addr[0] != clients[cli_idx].ip_address:
                    """BAD IP_ADDRESS DETECTED"""
                    pdu = Pdu(REGISTER_NACK, '00000000000', '000000000000', '000000',
                              'Motiu rebuig: Equip ' + received_pdu.system_id + 'amb adreça ip incorrecta')
            if pdu is not None:
                sockfd_udp.sendto(pdu.convert_pdu_to_pkg(UDP_PKG_SIZE), addr)
    except socket.timeout:
        pass


def maintenance_control(cli_idx, received_pdu):
    global sock_udp, clients, server
    non_recv_alives = 0
    while True:
        current_time = datetime.now()
        #print("CURRENT",current_time)
        try:
            data, addr = sockfd_udp.recvfrom(UDP_PKG_SIZE)
            received_pdu = convert_pkg_to_pdu(data)
            cli_idx = is_known_client(received_pdu.system_id)

            if received_pdu.pdu_type == ALIVE_INF:
                    clients[cli_idx].last_received_pkg = datetime.now()
                    if debug == 1:
                        print_time( f"DEBUG => Rebut: bytes={UDP_PKG_SIZE}, comanda={pdu_types[received_pdu.pdu_type]}, id={received_pdu.system_id}, mac={received_pdu.mac_address}, alea={received_pdu.alea}, dades={received_pdu.data}")
                    if cli_idx == -1:
                        pdu = Pdu(ALIVE_REJ, '00000000000', '000000000000', '000000','Motiu rebuig: Equip ' + received_pdu.system_id + 'no autoritzat o no registrat')
                        clients[cli_idx].status = status_names[DISCONNECTED]
                    elif addr[0] != clients[cli_idx].ip_address:
                        pdu = Pdu(ALIVE_NACK, '00000000000', '000000000000', '000000','Motiu rebuig: Adreça ip'+str(addr[0])+'incorrecta')
                        clients[cli_idx].status = status_names[DISCONNECTED]
                    elif not check_client_data(cli_idx, received_pdu):
                        pdu = Pdu(ALIVE_NACK, '00000000000', '000000000000', '000000', 'Motiu rebuig: Alea' + str(received_pdu.alea) + 'incorrecte')
                        clients[cli_idx].status = status_names[DISCONNECTED]
                    else:
                        pdu = Pdu(ALIVE_ACK, server_cfg.id, server_cfg.mac, received_pdu.alea,'')
                    if clients[cli_idx].status == "REGISTERED":
                        clients[cli_idx].status = status_names[ALIVE]

                    if debug == 1:
                        print_time( f"DEBUG =>  Enviat: bytes={UDP_PKG_SIZE}, comanda={pdu_types[pdu.pdu_type]}, id={server_cfg.id}, mac={server_cfg.mac}, alea={received_pdu.alea}, dades={pdu.data}")

                    sockfd_udp.sendto(pdu.convert_pdu_to_pkg(UDP_PKG_SIZE), addr)
        except socket.timeout:
            #print("last-receivedpkg", clients[cli_idx].last_received_pkg)
            #print_time((current_time - clients[cli_idx].last_received_pkg).total_seconds())
            if clients[cli_idx].status == 'REGISTERED' and (current_time - clients[cli_idx].last_received_pkg).total_seconds() >= R * J:
                clients[cli_idx].status = status_names[DISCONNECTED]
                print_time(f"MSG.  => {received_pdu.system_id} Equip  passa a estat: DISCONNECTED (No s'han rebut primer ALIVE en 4 segons)")
                break
            elif clients[cli_idx].status == "ALIVE" and (current_time - clients[cli_idx].last_received_pkg).total_seconds() >= R:
                non_recv_alives += 1
                if non_recv_alives >= S:
                    clients[cli_idx].status = status_names[DISCONNECTED]
                    break


def read_command_line():
    global clients, status_names
    while True:
        command = input()
        if command == 'list':
            print("\t\tNom \t|\t Mac \t  |\t   Estat \t|\t Adreça IP \t|\t Alea")
            for i in range(len(clients)):
                ip_addr_alea = ''
                if clients[i].status == "REGISTERED" or clients[i].status == "SEND_ALIVE":
                    ip_addr_alea = clients[i].ip_address + "\t\t" + clients[i].alea
                print(f"\t\t" + str(clients[i].id) + "\t" + str(clients[i].mac) + "\t" + clients[
                    i].status + "\t\t" + ip_addr_alea)
        elif command == 'quit':
            if debug == 1:
                print_time("DEBUG =>  Petició de finalització")
                print_time("DEBUG =>  Cancelat temporitzador per control alives")

            sys.exit()
        else:
            print("Comanda no reconeguda")


def is_known_client(id):
    global clients
    for i in range(len(clients)):
        if clients[i].id == id:
            return i
    return -1


def check_client_data(client_index, received_pdu):
    #checks id, mac and alea
    global clients
    return clients[client_index].id == received_pdu.system_id and clients[
        client_index].mac == received_pdu.mac_address and clients[client_index].alea == received_pdu.alea


def alea_generator():
    random_number = ""
    for i in range(6):
        random_number = random_number + str(random.randint(0, 9))
    return random_number


def create_udp_socket():
    global server, debug, sock_udp
    server_ip = "127.0.0.1"
    sockfd_udp = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    #sockfd_udp.setblocking(False)
    sockfd_udp.settimeout(2)
    try:
        sockfd_udp.bind((server_ip, int(server_cfg.udp_port)))
        if debug == 1:
            print_time('DEBUG =>  Socket UDP actiu')
    except:
        print_time("WARN. =>  No es pot fer bind amb el socket UDP")
        sys.exit(-1)
    return sockfd_udp


def create_tcp_socket():
    global server, debug, sock_tcp
    server_ip = "127.0.0.1"
    sockfd_tcp = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
    try:
        sockfd_tcp.bind((server_ip, int(server_cfg.tcp_port)))
        if debug == 1:
            print_time('DEBUG =>  Socket TCP actiu')
    except:
        print_time("WARN. =>  No es pot fer bind amb el socket TCP")
        sys.exit(-1)
    return sockfd_tcp


if __name__ == '__main__':
    main()
    # try:
    #     main()
    # except KeyboardInterrupt:
    #     sys.exit('Program terminated by user')
    # except SystemExit:
    #     sys.exit('Program terminated by sys.exit()')
    # except Exception as e:
    #     print(f'ERROR: {str(e)}')
    #     sys.exit('Program terminated with error')


