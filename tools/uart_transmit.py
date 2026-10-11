
import serial
import time
import sys

PORT = "/dev/ttyACM0"
BAUD = 115200

CHUNK_SIZE = 128
TEST_SIZE = 256
TIMEOUT = 5


def read_response(ser):
    response = ser.readline().decode(errors="replace").strip()
    print("STM32:", response)

    if not response:
        raise TimeoutError("No response received from STM32")

    return response


def main():
    if TEST_SIZE <= 0:
        raise ValueError("TEST_SIZE must be greater than zero")

    if CHUNK_SIZE <= 0:
        raise ValueError("CHUNK_SIZE must be greater than zero")

    print(f"Opening {PORT} at {BAUD} baud...")

    try:
        with serial.Serial(
            PORT,
            BAUD,
            timeout=TIMEOUT,
            write_timeout=TIMEOUT,
        ) as ser:

            # Allow the serial connection to settle.
            time.sleep(2)
            ser.reset_input_buffer()

            print(f"Starting test transfer: {TEST_SIZE} bytes")

            # Step 1: Request firmware update.
            command = f"BEGIN_UPDATE {TEST_SIZE}\r\n"
            ser.write(command.encode("ascii"))
            ser.flush()

            # Step 2: Wait for the STM32 to prepare Flash.
            response = read_response(ser)

            if response != "PREPARING":
                raise RuntimeError(
                    f"Expected PREPARING, received {response!r}"
                )

            # Step 3: Wait until Flash erase has completed.
            response = read_response(ser)

            if response != "READY":
                raise RuntimeError(
                    f"Expected READY, received {response!r}"
                )

            # Step 4: Generate dummy binary data.
            # This is NOT a valid STM32 application firmware image.
            data = bytes(range(256)) * (TEST_SIZE // 256)
            data = data[:TEST_SIZE]

            # Step 5: Send chunks and wait for each ACK.
            for offset in range(0, TEST_SIZE, CHUNK_SIZE):
                chunk = data[offset:offset + CHUNK_SIZE]

                ser.write(chunk)
                ser.flush()

                response = read_response(ser)

                if response != "ACK":
                    raise RuntimeError(
                        f"Expected ACK at offset {offset}, "
                        f"received {response!r}"
                    )

                print(
                    f"Sent {offset + len(chunk)}/{TEST_SIZE} bytes"
                )

            # Step 6: Confirm transfer completion.
            response = read_response(ser)

            if response != "UPDATE_OK":
                raise RuntimeError(
                    f"Expected UPDATE_OK, received {response!r}"
                )

            print("\nUART transfer and Flash programming test passed.")
            print("Note: dummy data was written, not valid firmware.")

    except serial.SerialException as exc:
        print(f"Serial error: {exc}", file=sys.stderr)
        sys.exit(1)

    except (RuntimeError, TimeoutError, ValueError) as exc:
        print(f"Transfer failed: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()