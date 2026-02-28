package main

import (
	"fmt"
	"time"

	"go.bug.st/serial"
)

type DLPIO8G struct {
	port serial.Port
}

func NewDLPIO8G(device string, baudrate int) (*DLPIO8G, error) {
	mode := &serial.Mode{
		BaudRate: baudrate,
		DataBits: 8,
		Parity:   serial.NoParity,
		StopBits: serial.OneStopBit,
	}

	port, err := serial.Open(device, mode)
	if err != nil {
		return nil, err
	}

	d := &DLPIO8G{port: port}

	// Ping
	pingCmd := []byte{0x27} // '
	_, err = port.Write(pingCmd)
	if err != nil {
		port.Close()
		return nil, err
	}

	buf := make([]byte, 1)
	n, err := port.Read(buf)
	if err != nil || n != 1 || buf[0] != 'Q' {
		port.Close()
		return nil, fmt.Errorf("device did not respond to ping correctly")
	}

	// Binary mode
	binaryCmd := []byte{0x5C} // \
	_, err = port.Write(binaryCmd)
	if err != nil {
		port.Close()
		return nil, err
	}

	return d, nil
}

func (d *DLPIO8G) Close() {
	if d.port != nil {
		d.port.Close()
	}
}

func (d *DLPIO8G) Ping() bool {
	pingCmd := []byte{0x27}
	_, err := d.port.Write(pingCmd)
	if err != nil {
		return false
	}

	buf := make([]byte, 1)
	n, err := d.port.Read(buf)
	return err == nil && n == 1 && buf[0] == 'Q'
}

func (d *DLPIO8G) Set(lines string) {
	_, err := d.port.Write([]byte(lines))
	if err != nil {
		fmt.Printf("write error in dlp Set: %v\n", err)
	}
}

func (d *DLPIO8G) Unset(lines string) {
	cmd := []byte(lines)
	for i := range cmd {
		switch cmd[i] {
		case '1':
			cmd[i] = 'Q'
		case '2':
			cmd[i] = 'W'
		case '3':
			cmd[i] = 'E'
		case '4':
			cmd[i] = 'R'
		case '5':
			cmd[i] = 'T'
		case '6':
			cmd[i] = 'Y'
		case '7':
			cmd[i] = 'U'
		case '8':
			cmd[i] = 'I'
		}
	}
	_, err := d.port.Write(cmd)
	if err != nil {
		fmt.Printf("write error in dlp Unset: %v\n", err)
	}
}

func (d *DLPIO8G) Delay(ms int) {
	time.Sleep(time.Duration(ms) * time.Millisecond)
}
