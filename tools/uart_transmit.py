from pathlib import Path
import serial
import time
import sys
import zlib

PORT = "/dev/ttyACM0"
BAUD = 115200
CHUNK_SIZE = 128
TIMEOUT = 5

MAX_APP_SIZE = 491520

APP_BIN = (
    Path(__file__).resolve().parent.parent
    / "application"
    / "application.bin"
)


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
    if not APP_BIN.is_file():
        raise FileNotFoundError(
            f"Application binary not found: {APP_BIN}"
        )

    data = APP_BIN.read_bytes()

    if not 0 < len(data) <= MAX_APP_SIZE:
        raise ValueError(
            f"Application size must be between 1 and "
            f"{MAX_APP_SIZE} bytes; got {len(data)}"
        )

    if CHUNK_SIZE != 128:
        raise ValueError("STM32 currently expects 128-byte chunks")

    expected_crc = zlib.crc32(data) & 0xFFFFFFFF

    print(f"Application: {APP_BIN}")
    print(f"Image size: {len(data)} bytes")
    print(f"CRC32: {expected_crc:08X}")
    print(f"Opening {PORT} at {BAUD} baud...")

    ser = None

    try:
        ser = serial.Serial(
            PORT,
            BAUD,
            timeout=TIMEOUT,
            write_timeout=TIMEOUT,
        )

        # Allow the port/board to settle, then discard stale startup output.
        time.sleep(2)
        ser.reset_input_buffer()

        command = (
            f"BEGIN_UPDATE {len(data)} {expected_crc:08X}\r"
        )
        print("Host command:", command.strip())

        ser.write(command.encode("ascii"))
        ser.flush()

        # The bootloader erases application Flash before replying READY.
        read_expected(ser, "PREPARING")
        read_expected(ser, "READY")

        # Send the image in the chunk size expected by the bootloader.
        for offset in range(0, len(data), CHUNK_SIZE):
            chunk = data[offset:offset + CHUNK_SIZE]

            ser.write(chunk)
            ser.flush()

            read_expected(ser, "ACK")
            print(f"Sent {offset + len(chunk)}/{len(data)} bytes")

        # Read the CRC diagnostics printed by the STM32.
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

        device_expected = int(
            expected_line[len(expected_prefix):], 16
        )
        device_actual = int(
            actual_line[len(actual_prefix):], 16
        )

        print(f"Host CRC:       {expected_crc:08X}")
        print(f"STM32 expected: {device_expected:08X}")
        print(f"STM32 actual:   {device_actual:08X}")

        if device_expected != expected_crc:
            raise RuntimeError(
                "Host and STM32 expected CRC values differ"
            )

        if device_actual != expected_crc:
            # The bootloader should report CRC_MISMATCH.
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

        print("\nFirmware transfer and CRC-32 verification passed.")
        print("The STM32 accepted the application image.")

    except (
        serial.SerialException,
        RuntimeError,
        TimeoutError,
        ValueError,
        OSError,
    ) as exc:
        print(f"Transfer failed: {exc}", file=sys.stderr)
        sys.exit(1)

    finally:
        if ser is not None and ser.is_open:
            ser.close()


if __name__ == "__main__":
    main()
