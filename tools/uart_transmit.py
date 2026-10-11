import serial
import time
import sys
import zlib

PORT = "/dev/ttyACM0"
BAUD = 115200
CHUNK_SIZE = 128
TEST_SIZE = 256
TIMEOUT = 5


def read_response(ser):
    response = ser.readline().decode("ascii", errors="replace").strip()
    print("STM32:", response)

    if not response:
        raise TimeoutError("No response received from STM32")

    if response.startswith("ERROR:"):
        raise RuntimeError(response)

    return response


def expect_response(ser, expected):
    response = read_response(ser)

    if response != expected:
        raise RuntimeError(
            f"Expected {expected!r}, received {response!r}"
        )

    return response


def main():
    if not 0 < TEST_SIZE <= 491520:
        raise ValueError("TEST_SIZE must be between 1 and 491520 bytes")

    if CHUNK_SIZE != 128:
        raise ValueError("STM32 currently expects 128-byte chunks")

    # Deterministic test pattern. This is NOT executable firmware.
    pattern = bytes(range(256))
    data = (pattern * ((TEST_SIZE + 255) // 256))[:TEST_SIZE]
    expected_crc = zlib.crc32(data) & 0xFFFFFFFF

    print(f"Image size: {len(data)} bytes")
    print(f"CRC32: {expected_crc:08X}")
    print(f"Opening {PORT} at {BAUD} baud...")

    ser = None

    try:
        # Create the serial object without opening the port yet.
        ser = serial.Serial(
            port=None,
            baudrate=BAUD,
            timeout=TIMEOUT,
            write_timeout=TIMEOUT,
        )

        # Set modem-control lines before opening the port to avoid
        # toggling DTR/RTS during port initialization.
        ser.port = PORT
        ser.dtr = False
        ser.rts = False
        ser.open()

        time.sleep(2)
        ser.reset_input_buffer()

        command = f"BEGIN_UPDATE {len(data)} {expected_crc:08X}\r\n"
        print("Host command:", command.strip())

        ser.write(command.encode("ascii"))
        ser.flush()

        expect_response(ser, "PREPARING")
        expect_response(ser, "READY")

        for offset in range(0, len(data), CHUNK_SIZE):
            chunk = data[offset:offset + CHUNK_SIZE]
            ser.write(chunk)
            ser.flush()

            expect_response(ser, "ACK")
            print(f"Sent {offset + len(chunk)}/{len(data)} bytes")

        expected_line = read_response(ser)
        actual_line = read_response(ser)

        expected_prefix = "EXPECTED_CRC: "
        actual_prefix = "ACTUAL_CRC: "

        if not expected_line.startswith(expected_prefix):
            raise RuntimeError(
                f"Expected CRC diagnostic, received {expected_line!r}"
            )

        if not actual_line.startswith(actual_prefix):
            raise RuntimeError(
                f"Expected actual CRC diagnostic, received {actual_line!r}"
            )

        device_expected = int(expected_line[len(expected_prefix):], 16)
        device_actual = int(actual_line[len(actual_prefix):], 16)

        print(f"Host CRC:       {expected_crc:08X}")
        print(f"STM32 expected: {device_expected:08X}")
        print(f"STM32 actual:   {device_actual:08X}")

        if device_expected != expected_crc:
            raise RuntimeError(
                "Host and STM32 expected CRC values differ"
            )

        if device_actual != expected_crc:
            # The bootloader normally sends CRC_MISMATCH next.
            try:
                read_response(ser)
            except RuntimeError as exc:
                if "CRC_MISMATCH" not in str(exc):
                    raise

            raise RuntimeError("CRC verification failed")

        expect_response(ser, "UPDATE_OK")

        print("\nCRC-32 verification passed.")
        print("Dummy data was written; it is not firmware.")

    except (
        serial.SerialException,
        RuntimeError,
        TimeoutError,
        ValueError,
    ) as exc:
        print(f"Transfer failed: {exc}", file=sys.stderr)
        sys.exit(1)

    finally:
        if ser is not None and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
