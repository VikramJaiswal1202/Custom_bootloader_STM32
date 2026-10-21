import serial
import time
import sys
import zlib

PORT = "/dev/ttyACM0"
BAUD = 115200
CHUNK_SIZE = 128
TEST_SIZE = 256
TIMEOUT = 5


def read_line(ser):
    """Read one newline-terminated line and return stripped text."""
    raw = ser.readline()

    if not raw:
        raise TimeoutError("No response received from STM32")

    response = raw.decode("ascii", errors="replace").strip()
    print("STM32:", response)
    return response


def read_expected(ser, expected, ignore_debug=True):
    """
    Wait for an expected protocol response.
    Ignore CMD=[...] debug lines, but surface errors and unexpected lines.
    """
    deadline = time.monotonic() + TIMEOUT

    while time.monotonic() < deadline:
        remaining = deadline - time.monotonic()
        ser.timeout = max(0.1, remaining)

        try:
            response = read_line(ser)
        except TimeoutError:
            break

        if ignore_debug and response.startswith("CMD=["):
            continue

        if response.startswith("ERROR:"):
            raise RuntimeError(response)

        if response == expected:
            return response

        raise RuntimeError(
            f"Expected {expected!r}, received {response!r}"
        )

    raise TimeoutError(
        f"Timed out waiting for {expected!r} from STM32"
    )


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
        # Match the simple diagnostic that successfully received GET_INFO.
        # Use normal pyserial port initialization.
        ser = serial.Serial(
            PORT,
            BAUD,
            timeout=TIMEOUT,
            write_timeout=TIMEOUT,
        )

        # Allow the port/board to settle, then discard stale startup output.
        time.sleep(2)
        ser.reset_input_buffer()

        command = f"BEGIN_UPDATE {len(data)} {expected_crc:08X}\r"
        print("Host command:", command.strip())
        ser.write(command.encode("ascii"))
        ser.flush()

        read_expected(ser, "PREPARING")
        read_expected(ser, "READY")

        for offset in range(0, len(data), CHUNK_SIZE):
            chunk = data[offset:offset + CHUNK_SIZE]
            ser.write(chunk)
            ser.flush()

            read_expected(ser, "ACK")
            print(f"Sent {offset + len(chunk)}/{len(data)} bytes")

        expected_line = read_line(ser)
        actual_line = read_line(ser)

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
            raise RuntimeError("Host and STM32 expected CRC values differ")

        if device_actual != expected_crc:
            # The bootloader normally sends ERROR: CRC_MISMATCH next.
            try:
                response = read_line(ser)
                if "CRC_MISMATCH" not in response:
                    raise RuntimeError(
                        f"Expected CRC_MISMATCH, received {response!r}"
                    )
            except TimeoutError:
                pass
            raise RuntimeError("CRC verification failed")

        read_expected(ser, "UPDATE_OK")
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
