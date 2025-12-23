# FSP Server for Swiss 

This is a lightweight FSP server meant for streaming games to a Gamecube running Swiss via the broadband adapter.

## Usage
    fsp_server.exe -d [directory] [additional options]
      options:
        -d, --directory:         Determines the directory in which the server should run.
        -p, --password:          Sets a password that clients need to supply to access files. [Default: none]
        -a, --address:           Determines which address the socket should be bound to. [Default: 0:0.0:0:21]
        -i  -ignore-keys;        Determines whether or not FSP packet key validation should be skipped. [Default: 1]
        -v, --version:           Display version info.

