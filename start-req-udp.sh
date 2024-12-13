export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
export VSOMEIP_CONFIGURATION=../../config/vsomeip-udp-client.json
export VSOMEIP_APPLICATION_NAME=client-sample
sudo route add -host 224.224.224.245 dev enp0s8
./request-sample
