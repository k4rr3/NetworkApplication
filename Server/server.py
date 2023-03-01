def main():
    read_server_cfg()
    read_known_devices()


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


server_cfg = None
known_devices = []


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


main()
