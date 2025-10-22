import json

if __name__ == "__main__":
    config = {}

    base = {}
    base["base_port"] = 5600
    base["host"] = "127.0.0.1"
    base["port"] = 11000

    scanner = {}
    scanner["begin"] = 5600
    scanner["end"] = 5620
    scanner["scan_interval"] = 5

    config["base"] = base
    config["scanner"] = scanner

    with open("config/config.json", "w", encoding="utf-8") as f:
        json.dump(config, f, indent=4)
