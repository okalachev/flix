#ifdef ESP32
#include <Preferences.h>
#else

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>

class Preferences {
public:
	Preferences() = default;
	~Preferences() = default;

	bool begin(const char *name, bool readOnly = false, const char *partition_label = NULL) {
		(void)name;
		(void)readOnly;
		(void)partition_label;
		load();
		started = true;
		return true;
	}

	void end() {
		started = false;
	}

	bool clear() {
		if (!started) return false;
		for (auto &entry : entries) {
			entry = Entry();
		}
		return save();
	}

	size_t putFloat(const char *key, float value) {
		if (!started) return 0;
		int index = ensureKey(key);
		if (index < 0) return 0;
		entries[index].hasFloat = true;
		entries[index].hasString = false;
		entries[index].stringValue = "";
		entries[index].floatValue = value;
		if (!save()) return 0;
		return sizeof(float);
	}

	float getFloat(const char *key, float defaultValue = NAN) {
		if (!started) return defaultValue;
		int index = findKey(key);
		if (index < 0 || !entries[index].hasFloat) return defaultValue;
		return entries[index].floatValue;
	}

	size_t putString(const char *key, const char *value) {
		if (!started) return 0;
		int index = ensureKey(key);
		if (index < 0) return 0;
		if (!value) value = "";
		if (strlen(value) > MAX_STRING_LEN) return 0;

		entries[index].hasString = true;
		entries[index].hasFloat = false;
		entries[index].stringValue = value;
		if (!save()) return 0;
		return entries[index].stringValue.length();
	}

	size_t putString(const char *key, String value) {
		return putString(key, value.c_str());
	}

	String getString(const char *key, String defaultValue = String()) {
		if (!started) return defaultValue;
		int index = findKey(key);
		if (index < 0 || !entries[index].hasString) return defaultValue;
		return entries[index].stringValue;
	}

	bool isKey(const char *key) {
		if (!started) return false;
		return findKey(key) >= 0;
	}

private:
	static const int MAX_ENTRIES = 128;
	static const int MAX_KEY_LEN = 15;
	static const int MAX_STRING_LEN = 95;
	static const uint32_t MAGIC = 0x46504B56; // "VKPF"
	static const uint8_t VERSION = 1;
	static const uint8_t TYPE_FLOAT = 1;
	static const uint8_t TYPE_STRING = 2;
	static const int STORAGE_SIZE = 4096;

	struct Entry {
		bool used = false;
		char key[MAX_KEY_LEN + 1] = {};
		bool hasFloat = false;
		float floatValue = NAN;
		bool hasString = false;
		String stringValue;
	};

	bool started = false;
	Entry entries[MAX_ENTRIES];

	static void writeU16(uint8_t *dst, uint16_t value) {
		dst[0] = static_cast<uint8_t>(value & 0xFF);
		dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
	}

	static void writeU32(uint8_t *dst, uint32_t value) {
		dst[0] = static_cast<uint8_t>(value & 0xFF);
		dst[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
		dst[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
		dst[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
	}

	static uint16_t readU16(const uint8_t *src) {
		return static_cast<uint16_t>(src[0]) |
			(static_cast<uint16_t>(src[1]) << 8);
	}

	static uint32_t readU32(const uint8_t *src) {
		return static_cast<uint32_t>(src[0]) |
			(static_cast<uint32_t>(src[1]) << 8) |
			(static_cast<uint32_t>(src[2]) << 16) |
			(static_cast<uint32_t>(src[3]) << 24);
	}

	int availableStorageSize() {
		int n = STORAGE_SIZE;
		if (EEPROM.length() > 0 && EEPROM.length() < n) n = EEPROM.length();
		return n;
	}

	bool save() {
		const int storageSize = availableStorageSize();
		if (storageSize < 16) return false;

		uint8_t buffer[STORAGE_SIZE] = {};
		int pos = 0;
		writeU32(buffer + pos, MAGIC);
		pos += 4;
		buffer[pos++] = VERSION;
		buffer[pos++] = 0;
		int lengthPos = pos;
		pos += 2;

		for (int i = 0; i < MAX_ENTRIES; i++) {
			if (!entries[i].used) continue;
			if (!entries[i].hasFloat && !entries[i].hasString) continue;

			const uint8_t keyLen = static_cast<uint8_t>(strnlen(entries[i].key, MAX_KEY_LEN));
			if (keyLen == 0) continue;

			if (entries[i].hasFloat) {
				const int recordSize = 3 + keyLen + 4;
				if (pos + recordSize > storageSize) return false;

				buffer[pos++] = TYPE_FLOAT;
				buffer[pos++] = keyLen;
				buffer[pos++] = 4;
				memcpy(buffer + pos, entries[i].key, keyLen);
				pos += keyLen;
				float value = entries[i].floatValue;
				memcpy(buffer + pos, &value, sizeof(value));
				pos += sizeof(value);
			} else if (entries[i].hasString) {
				const uint8_t valueLen = static_cast<uint8_t>(entries[i].stringValue.length());
				const int recordSize = 3 + keyLen + valueLen;
				if (pos + recordSize > storageSize) return false;

				buffer[pos++] = TYPE_STRING;
				buffer[pos++] = keyLen;
				buffer[pos++] = valueLen;
				memcpy(buffer + pos, entries[i].key, keyLen);
				pos += keyLen;
				if (valueLen > 0) {
					memcpy(buffer + pos, entries[i].stringValue.c_str(), valueLen);
					pos += valueLen;
				}
			}
		}

		writeU16(buffer + lengthPos, static_cast<uint16_t>(pos));

		for (int i = 0; i < storageSize; i++) {
			EEPROM.write(i, buffer[i]);
		}
		EEPROM.commit();
		return true;
	}

	void load() {
		const int storageSize = availableStorageSize();
		if (storageSize < 16) return;

		for (auto &entry : entries) entry = Entry();

		uint8_t buffer[STORAGE_SIZE] = {};
		for (int i = 0; i < storageSize; i++) {
			buffer[i] = EEPROM.read(i);
		}

		int pos = 0;
		if (readU32(buffer + pos) != MAGIC) return;
		pos += 4;
		if (buffer[pos++] != VERSION) return;
		pos++; // flags

		const uint16_t totalLen = readU16(buffer + pos);
		pos += 2;
		if (totalLen < pos || totalLen > storageSize) return;

		while (pos + 3 <= totalLen) {
			const uint8_t type = buffer[pos++];
			const uint8_t keyLen = buffer[pos++];
			const uint8_t valueLen = buffer[pos++];
			if (keyLen == 0 || keyLen > MAX_KEY_LEN) return;
			if (pos + keyLen + valueLen > totalLen) return;

			char key[MAX_KEY_LEN + 1] = {};
			memcpy(key, buffer + pos, keyLen);
			key[keyLen] = '\0';
			pos += keyLen;

			int index = ensureKey(key);
			if (index < 0) return;

			if (type == TYPE_FLOAT && valueLen == 4) {
				float value = NAN;
				memcpy(&value, buffer + pos, sizeof(value));
				entries[index].hasFloat = true;
				entries[index].hasString = false;
				entries[index].stringValue = "";
				entries[index].floatValue = value;
			} else if (type == TYPE_STRING && valueLen <= MAX_STRING_LEN) {
				char value[MAX_STRING_LEN + 1] = {};
				if (valueLen > 0) memcpy(value, buffer + pos, valueLen);
				value[valueLen] = '\0';
				entries[index].hasString = true;
				entries[index].hasFloat = false;
				entries[index].stringValue = value;
			}

			pos += valueLen;
		}
	}

	int findKey(const char *key) {
		if (!key) return -1;
		for (int i = 0; i < MAX_ENTRIES; i++) {
			if (!entries[i].used) continue;
			if (strncmp(entries[i].key, key, MAX_KEY_LEN + 1) == 0) return i;
		}
		return -1;
	}

	int ensureKey(const char *key) {
		if (!key) return -1;
		if (strlen(key) > MAX_KEY_LEN) return -1;

		int index = findKey(key);
		if (index >= 0) return index;

		for (int i = 0; i < MAX_ENTRIES; i++) {
			if (entries[i].used) continue;
			entries[i].used = true;
			entries[i].hasFloat = false;
			entries[i].hasString = false;
			entries[i].floatValue = NAN;
			entries[i].stringValue = "";
			strncpy(entries[i].key, key, MAX_KEY_LEN);
			entries[i].key[MAX_KEY_LEN] = '\0';
			return i;
		}

		return -1;
	}
};


#endif
