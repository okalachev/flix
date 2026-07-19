// Dummy file for the simulator

class ESP_NOW_Peer {
protected:
	size_t send(const uint8_t *data, int len) { return 0; }
};

class ESP_NOW_Serial_Class : public ESP_NOW_Peer {
public:
	virtual void onSent(bool success) {};
	virtual size_t write(const uint8_t *data, size_t len) { return 0; };
};
