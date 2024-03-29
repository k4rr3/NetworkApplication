#!/usr/bin/python3
import os
import random
import select
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

R = 2
J = 2
S = 3
W = 3
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

devices_file = 'equips.dat'
server_file = 'server.cfg'
debug = 0
server = None
clients = []
threads = []
UDP_SIZE = 78
TCP_SIZE = 178
sock_udp = -1
sock_tcp = -1


class Pdu:
    def __init__(self, type, id, mac, alea, data):
        self.type = type
        self.id = id
        self.mac = mac
        self.alea = alea
        self.data = data

    def convert_pdu_to_pkg(self, pkg_size):
        pdu = [chr(0)] * pkg_size
        pdu[0] = chr(self.type)
        pdu[1: 1 + len(self.id)] = self.id
        pdu[8:8 + len(self.mac)] = self.mac
        pdu[21:21 + len(self.alea)] = self.alea
        pdu[28:28 + len(self.data)] = self.data
        pdu = ''.join(pdu).encode()
        return pdu


def convert_pkg_to_pdu(package):
    id = str(package[1:7].decode()).rstrip('\x00')
    mac = package[8:20].decode().rstrip('\x00')
    alea = package[21:28].decode().rstrip('\x00')
    try:
        data = package[28:].decode().rstrip('\x00')
    except:
        data = ""
        for i in package[28:]:
            if chr(i) == '\n' or chr(i) == '\x00':
                break
            data = data + chr(i)
    return Pdu(package[0], id, mac, alea, data)


class Cfg:
    def __init__(self, id, mac, udp_port, tcp_port):
        self.id = id
        self.mac = mac
        self.udp_port = udp_port
        self.tcp_port = tcp_port


class KnownDevice:
    def __init__(self, id, mac, alea, ip, status, last_received, non_received_alives, is_transfering_files):
        self.id = id
        self.mac = mac
        self.status = status
        self.alea = alea
        self.ip = ip
        self.last_received = last_received
        self.non_received_alives = non_received_alives
        self.is_transfering_files = is_transfering_files


def main():
    global sock_udp, sock_tcp
    read_args_cfg_files()
    read_known_clients()
    read_server_cfg()

    sock_udp = create_udp_socket()
    sock_tcp = create_tcp_socket()
    service_loop()


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
        print_time("DEBUG =>  Llegits paràmetres línia de comandes ")


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
            # server id        mac      udp_port   tcp_port
    server = Cfg(cfg[0][1], cfg[1][1], cfg[2][1], cfg[3][1])
    if debug == 1:
        print_time('DEBUG =>  Llegits paràmetres arxiu de configuració')


def read_known_clients():
    global clients, devices_file
    equips_dat_file = open(devices_file, "r")
    cfg = []
    line = " "
    while line != '':
        line = equips_dat_file.readline()
        if line != '\n' and line != '':
            cfg.append(line.split())
            clients.append(KnownDevice(cfg[0][0], cfg[0][1], None, None, status_names[DISCONNECTED], None, 0, False))
            cfg.pop()
    print_time("INFO  =>  Llegits 9 equips autoritzats en el sistema")


def service_loop():
    global sock_udp, sock_tcp
    # Use select to wait for input from stdin, UDP socket, and TCP socket
    inputs = [sys.stdin, sock_udp, sock_tcp]
    if debug == 1:
        print_time("DEBUG =>  Creat fill per gestionar alives")
    print_time("INFO  =>  Establert temporitzador per control alives")
    while True:
        # Wait for any of the input sources to become readable
        readable, _, _ = select.select(inputs, [], [], 0)
        check_timeout()
        for r in readable:
            # Handle input from stdin
            if r is sys.stdin:
                command_line_phase()

            # Handle input from UDP socket
            elif r is sock_udp:
                data, addr = sock_udp.recvfrom(UDP_SIZE)
                received_pdu = convert_pkg_to_pdu(data)
                if received_pdu.type == REGISTER_REQ:
                    thread_register_req = threading.Thread(target=register_phase, args=(received_pdu, data, addr))
                    threads.append(thread_register_req)
                    thread_register_req.start()
                elif received_pdu.type == ALIVE_INF:
                    thread_alive = threading.Thread(target=alive_phase, args=(received_pdu, addr))
                    threads.append(thread_alive)
                    thread_alive.start()

            # Handle input from TCP socket
            elif r is sock_tcp:
                conn, addr = r.accept()
                tcp_thread = threading.Thread(target=tcp_phase, args=(conn, addr))
                threads.append(tcp_thread)
                tcp_thread.start()


def register_phase(received_pdu, data, addr):
    global UDP_SIZE, clients, pdu_types, server, sock_udp
    pdu = None
    if debug == 1:
        print_time(f"DEBUG =>  Rebut: bytes={len(data)}, comanda={pdu_types[received_pdu.type]}, id={received_pdu.id}, mac={received_pdu.mac}, alea={received_pdu.alea}, dades={received_pdu.data}")

    i = is_known_client(received_pdu.id)  # get the index of the client, if it's known
    if i == -1:
        pdu = Pdu(REGISTER_REJ, '', '000000000000', '000000','Equip ' + received_pdu.id + ' no autoritzat')

    elif (clients[i].status == 'REGISTERED' or clients[i].status == 'ALIVE') and check_client_data(i, received_pdu):
        clients[i].last_received = datetime.now()  # saving register time
        clients[i].non_received_alives = 0  # reset the number of non_received_alives to 0
        pdu = Pdu(REGISTER_ACK, clients[i].id, clients[i].mac, clients[i].alea, server.tcp_port)
        print_time(f"INFO  =>  Acceptat registre duplicat. Equip: id={clients[i].id}, ip={clients[i].ip}, mac={clients[i].mac} alea={received_pdu.alea}")
        print_time(f"MSG.  =>  Equip Sw-001 passa a estat:{status_names[REGISTERED]}")

    elif clients[i].status == "DISCONNECTED" or clients[i].status == "WAIT_DB_CHECK":
        if (received_pdu.alea != '000000' and clients[i].alea is not None and received_pdu.alea != clients[i].alea) or received_pdu.alea != "000000":
            pdu = Pdu(REGISTER_NACK, '', '000000000000', '000000','Error en dades de registre')
            clients[i].status = status_names[WAIT_DB_CHECK]
            print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[WAIT_DB_CHECK])
            print_time(f"INFO  =>  Rebutjat registre. Equip: id={clients[i].id}, ip={addr[0]}, mac={clients[i].mac} alea={received_pdu.alea} (alea incorrecte)")

        elif received_pdu.alea == '000000':
            clients[i].status = status_names[WAIT_DB_CHECK]
            print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[WAIT_DB_CHECK])
            clients[i].alea = alea_generator()
            pdu = Pdu(REGISTER_ACK, server.id, server.mac, clients[i].alea, server.tcp_port)
            clients[i].ip = addr[0]
            clients[i].last_received = datetime.now()  # saving register time
            clients[i].status = status_names[REGISTERED]
            print_time('INFO  =>  Acceptat registre. Equip: id=' + clients[i].id + ', ip=' + clients[i].ip + ', mac=' + clients[i].mac + ' alea=' + received_pdu.alea)
            print_time('MSG.  =>  Equip Sw-001 passa a estat:' + status_names[REGISTERED])

        elif addr[0] != clients[i].ip:
            pdu = Pdu(REGISTER_NACK, '', '000000000000', '000000','Equip ' + received_pdu.id + 'amb adreça ip incorrecta')

    if pdu is not None:
        if debug == 1:
            print_time(f"DEBUG =>  Enviat: bytes={UDP_SIZE}, comanda={pdu_types[pdu.type]}, id={pdu.id}, mac={pdu.mac}, alea={pdu.alea}, dades={pdu.data}")

        sock_udp.sendto(pdu.convert_pdu_to_pkg(UDP_SIZE), addr)


def alive_phase(received_pdu, addr):
    global sock_udp, clients, server
    if debug == 1:
        print_time( f"DEBUG =>  Rebut: bytes={UDP_SIZE}, comanda={pdu_types[received_pdu.type]}, id={received_pdu.id}, mac={received_pdu.mac}, alea={received_pdu.alea}, dades={received_pdu.data}")
    i = is_known_client(received_pdu.id)  # get the index of the client if it's known
    clients[i].last_received = datetime.now()  # saving last alive time reception
    clients[i].non_received_alives = 0 # when ALIVE_INF is received, counter is reset to 0
    if i == -1 or clients[i].status == "DISCONNECTED":
        pdu = Pdu(ALIVE_REJ, '', '000000000000', '000000','Equip no autoritzat en el sistema')
        clients[i].status = status_names[DISCONNECTED]
        print_time(f"INFO  =>  Rebutjat ALIVE. Equip: id={received_pdu.id} ip={addr[0]} mac={received_pdu.mac}(no autoritzat)")
    elif addr[0] != clients[i].ip:
        pdu = Pdu(ALIVE_NACK, '', '000000000000', '000000','Adreça ip' + str(addr[0]) + 'incorrecta')
        clients[i].status = status_names[DISCONNECTED]
    elif not check_client_data(i, received_pdu):
        pdu = Pdu(ALIVE_NACK, '', '000000000000', '000000','Error enviament ALIVE (dades equip incorrectes)')
        print_time(f"INFO  =>  Error recepció ALIVE. Equip: id={received_pdu.id}, ip={addr[0]}, mac={received_pdu.mac} alea={received_pdu.alea} (Registrat: id={clients[i].id}, ip={clients[i].ip}, mac={clients[i].ip} alea={clients[i].alea})")
    else:
        pdu = Pdu(ALIVE_ACK, server.id, server.mac, received_pdu.alea, '')
        print_time(f"INFO  =>  Acceptat ALIVE. Equip: id={received_pdu.id}, ip={addr[0]}, mac={received_pdu.mac} alea={received_pdu.alea}")
    if clients[i].status == "REGISTERED":
        print_time(f"MSG.  =>  Equip {clients[i].id} passa a estat: SEND_ALIVE")
        clients[i].status = status_names[ALIVE]
    if debug == 1:
        print_time( f"DEBUG =>  Enviat: bytes={UDP_SIZE}, comanda={pdu_types[pdu.type]}, id={pdu.id}, mac={pdu.mac}, alea={pdu.alea}, dades={pdu.data}\n")

    sock_udp.sendto(pdu.convert_pdu_to_pkg(UDP_SIZE), addr)


def tcp_phase(conn, addr):
    global clients, server
    if debug == 1:
        print_time("DEBUG =>  Rebuda connexió TCP, creat procés per atendre'l")
    pdu = convert_pkg_to_pdu(conn.recv(TCP_SIZE))
    if debug == 1:
        print_time(
            f"DEBUG =>  Rebut: bytes={TCP_SIZE}, comanda={pdu_types[pdu.type]}, id={pdu.id}, mac={pdu.mac}, alea={pdu.alea}  dades={pdu.data}")
    i = is_known_client(pdu.id)
    if pdu.type == SEND_FILE:
        if check_client_data(i, pdu) and not clients[i].is_transfering_files:
            clients[i].is_transfering_files = True
            tcp_pdu = Pdu(SEND_ACK, server.id, server.mac, clients[i].alea, clients[i].id + ".cfg")
            conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
            if debug == 1:
                print_time(f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}")
            f = open(clients[i].id + ".cfg", "w")
            while pdu.type == SEND_DATA or pdu.type == SEND_FILE:
                ready = select.select([conn], [], [], W)
                if ready[0]:
                    pdu = convert_pkg_to_pdu(conn.recv(TCP_SIZE))
                    if debug == 1:
                        print_time(
                            f"DEBUG =>  Rebut: bytes={TCP_SIZE}, comanda={pdu_types[pdu.type]}, id={pdu.id}, mac={pdu.mac}, alea={pdu.alea}  dades={pdu.data}\n")

                    if pdu.type != SEND_END:
                        #  to check if data contains '\n' and add it if necessary
                        if len(pdu.data) > 0 and pdu.data[len(pdu.data) - 1] == '\n':
                            f.write(pdu.data)
                        else:
                            f.write(pdu.data + "\n")

                else:
                    print_time(f"ALERT =>  No s'ha rebut informació per el canal TCP durant {W} segons")
                    break

            print_time(f"MSG.  =>  Finalitzat enviament arxiu configuració. Equip: id={clients[i].id}, ip={addr[0]}, mac={pdu.mac} alea={pdu.alea}")
            if debug == 1:
                print_time("DEBUG =>  Finalitzat el procés que atenia a un client TCP")
            clients[i].is_transfering_files = False
        elif clients[i].id != pdu.id or clients[i].mac != pdu.mac:
            tcp_pdu = Pdu(SEND_REJ, '', '000000000000', '000000', 'Equip no autoritzat en el sistema')
            print_time("INFO  =>  Rebutjat paquet TCP tipus: SEND_FILE (Equip no autoritzat)")
            if debug == 1:
                print_time(f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}")
            conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
        elif clients[i].ip != addr[0] or clients[i].alea != pdu.alea or clients[i].is_transfering_files:
            tcp_pdu = Pdu(SEND_NACK, '', '000000000000', '000000', 'Rebutjat paquet TCP tipus: SEND_FILE (Dades addicionals de l’equip incorrectes)')
            if debug == 1:
                print_time(f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}")
            conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))

    elif pdu.type == GET_FILE:
        if check_client_data(i, pdu) and not clients[i].is_transfering_files:

            clients[i].is_transfering_files = True
            tcp_pdu = Pdu(GET_ACK, server.id, server.mac, clients[i].alea, clients[i].id + ".cfg")
            conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
            if debug == 1:
                print_time(
                    f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={server.id}, mac={server.mac}, alea={clients[i].alea}  dades={clients[i].id}.cfg")
            f = open(clients[i].id + ".cfg", "r")
            while pdu.type != GET_END:
                line = f.readline()
                if len(line) > 0:
                    tcp_pdu = Pdu(GET_DATA, server.id, server.mac, clients[i].alea, line)
                else:
                    tcp_pdu = Pdu(GET_END, server.id, server.mac, clients[i].alea, '')
                    if debug == 1:
                        print_time(
                            f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}\n")
                    conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
                    break
                conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
                if debug == 1:
                    print_time(
                        f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}")

            f.close()
            print_time(
                f"MSG.  =>  Finalitzat enviament arxiu configuració. Equip: id={clients[i].id}, ip={addr[0]}, mac={pdu.mac} alea={pdu.alea}")
            if debug == 1:
                print_time("DEBUG =>  Finalitzat el procés que atenia a un client TCP")
            clients[i].is_transfering_files = False
        elif clients[i].id != pdu.id or clients[i].mac != pdu.mac or not os.path.isfile(clients[i].id+".cfg"):
            tcp_pdu = Pdu(GET_REJ, '', '000000000000', '000000', 'Rebutjat paquet TCP tipus: GET_FILE (Equip no autoritzat)')
            print_time("INFO  =>  Denegat paquet TCP tipus: GET_FILE (Error dades equip)")
            if debug == 1:
                print_time(f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}")

            conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
        elif clients[i].ip != addr[0] or clients[i].alea != pdu.alea or clients[i].is_transfering_files:
            tcp_pdu = Pdu(GET_NACK, '', '000000000000', '000000', 'Rebutjat paquet TCP tipus: GET_FILE (Dades addicionals de l’equip incorrectes)')

            if debug == 1:
                print_time(f"DEBUG =>  Enviat: bytes={TCP_SIZE}, comanda={pdu_types[tcp_pdu.type]}, id={tcp_pdu.id}, mac={tcp_pdu.mac}, alea={tcp_pdu.alea}  dades={tcp_pdu.data}")
            conn.send(tcp_pdu.convert_pdu_to_pkg(TCP_SIZE))
    conn.close()


def command_line_phase():
    global clients, status_names, threads, sock_udp, sock_tcp
    command = input()
    if command == 'list':
        print("\t Nom \t|\t Mac \t  |\t   Estat \t|\t Adreça IP \t|\t Alea\n")
        for cli in clients:
            ip_addr_alea = ''
            if cli.status == "REGISTERED" or cli.status == "SEND_ALIVE":
                ip_addr_alea = cli.ip + "\t\t" + cli.alea
            ip_addr_alea = str(cli.ip) + "\t\t" + str(cli.alea)
            print(f"\t" + str(cli.id) + "\t    " + str(cli.mac) + "\t" + cli.status + "\t\t\t" + ip_addr_alea)
        print("\n")
    elif command == 'quit':
        if debug == 1:
            print_time("DEBUG =>  Petició de finalització")
            print_time("DEBUG =>  Cancelat temporitzador per control alives")
        close()
    else:
        print_time("MSG.  =>  Comanda incorrecta")


def check_timeout():
    global clients
    current_date = datetime.now()
    for client in clients:
        if (client.last_received is not None and client.status == "REGISTERED" and (current_date - client.last_received).total_seconds() > R * J) or (client.status == "ALIVE" and client.non_received_alives == S):
            client.status = "DISCONNECTED"
            if client.non_received_alives == S:
                print_time(f"MSG.  =>  Equip {client.id} passa a estat: DISCONNECTED (No s'han rebut 3 ALIVES consecutius)")
            else:
                print_time(f"MSG.  =>  Equip {client.id} passa a estat: DISCONNECTED (No s'han rebut primer ALIVE en 4 segons)")

        elif client.status == "ALIVE" and (current_date - client.last_received).total_seconds() >= R:
            client.last_received = current_date
            client.non_received_alives += 1


def is_known_client(id):
    global clients
    for i in range(len(clients)):
        if clients[i].id == id:
            return i
    return -1


def check_client_data(index, pdu):
    # checks id, mac and alea
    global clients
    return clients[index].id == pdu.id and clients[index].mac == pdu.mac and clients[index].alea == pdu.alea


def alea_generator():
    random_number = ""
    for i in range(6):
        random_number = random_number + str(random.randint(0, 9))
    return random_number


def create_udp_socket():
    global server, debug, sock_udp
    server_ip = "127.0.0.1"
    sock_udp = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
    try:
        sock_udp.bind((server_ip, int(server.udp_port)))
        if debug == 1:
            print_time('DEBUG =>  Socket UDP actiu')
    except:
        print_time("WARN. =>  No es pot fer bind amb el socket UDP")
        close()
    return sock_udp


def create_tcp_socket():
    global server, debug, sock_tcp
    server_ip = "127.0.0.1"
    sock_tcp = socket.socket(family=socket.AF_INET, type=socket.SOCK_STREAM)
    sock_tcp.setblocking(False)
    try:
        sock_tcp.bind((server_ip, int(server.tcp_port)))
        sock_tcp.listen(5)  # number inside defines how many connection requests could be in the queue
        if debug == 1:
            print_time('DEBUG =>  Socket TCP actiu')
    except:
        print_time("WARN. =>  No es pot fer bind amb el socket TCP")
        close()
    return sock_tcp


def close():
    global threads, sock_udp, sock_tcp
    if sock_udp != -1:
        sock_udp.close()
    if sock_tcp != -1:
        sock_tcp.close()
    for t in threads:
        t.join()
    sys.exit()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        if debug == 1:
            print_time("DEBUG =>  Finalització per ^C")
        close()
    except SystemExit:
        close()
    except Exception as e:
        print(f'ERROR: {str(e)}')
        close()
