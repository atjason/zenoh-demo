import time

import zenoh


KEY = "demo/zenoh/getting-started"


def main() -> None:
    conf = zenoh.Config()
    with zenoh.open(conf) as session:
        print(f"Opened zenoh session. Declaring Python publisher on key: {KEY}")
        pub = session.declare_publisher(KEY)

        count = 0
        try:
            while True:
                msg = f"Hello from Python #{count}"
                print(f"[Py pub] Putting Data ('{KEY}': '{msg}')")
                pub.put(msg)
                count += 1
                time.sleep(1.0)
        except KeyboardInterrupt:
            print("Shutting down Python publisher...")


if __name__ == "__main__":
    main()

