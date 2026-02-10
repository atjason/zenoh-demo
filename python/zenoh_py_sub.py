import time

import zenoh


KEY = "demo/zenoh/getting-started"


def listener(sample: zenoh.Sample) -> None:
    print(
        f"[Py sub] Received ('{sample.key_expr}': "
        f"'{sample.payload.to_string()}')"
    )


def main() -> None:
    conf = zenoh.Config()
    with zenoh.open(conf) as session:
        print(f"Opened zenoh session. Declaring Python subscriber on key: {KEY}")
        session.declare_subscriber(KEY, listener)

        try:
            # Keep the process alive while callbacks are being invoked.
            while True:
                time.sleep(1.0)
        except KeyboardInterrupt:
            print("Shutting down Python subscriber...")


if __name__ == "__main__":
    main()

