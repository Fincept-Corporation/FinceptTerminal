import os
import platform
import subprocess
from fincept_terminal.FinceptTerminalUtilsModule.themes import console
from rich.panel import Panel
from rich.syntax import Syntax

def show_finscript_menu():
    """Display the FinScript menu with options."""
    while True:
        console.print("[bold cyan]FINSCRIPT MENU[/bold cyan]\n", style="info")

        finscript_menu_text = """
1. Create New File
2. Open Existing File
3. Example Files
4. Documentation
5. Help
6. Back to Main Menu
        """

        menu_panel = Panel(finscript_menu_text, title="FinScript Options", title_align="center", style="bold green on #282828", padding=(1, 2))
        console.print(menu_panel)

        choice = input("Enter your choice: ")

        if choice == "1":
            check_and_install_micro()
            create_new_finscript_file()
        elif choice == "2":
            check_and_install_micro()
            open_existing_finscript_file()
        elif choice == "3":
            show_example_files()
        elif choice == "4":
            show_finscript_docs()
        elif choice == "5":
            show_finscript_help()
        elif choice == "6":
            from fincept_terminal.oldTerminal.cli import show_main_menu
            show_main_menu()
        else:
            console.print("\n[danger]INVALID OPTION. PLEASE TRY AGAIN.[/danger]")

def check_and_install_micro():
    """Check if Micro is installed and install it if necessary."""
    try:
        subprocess.run(["micro", "--version"], check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        console.print("[success]Micro editor is already installed on your system.[/success]")
    except (subprocess.CalledProcessError, FileNotFoundError):
        console.print("[warning]Micro editor is not installed. Installing it now...[/warning]")
        install_micro()

def install_micro():
    """Install Micro based on the detected operating system."""
    os_type = platform.system().lower()

    try:
        if os_type == "linux":
            distro = subprocess.run(["lsb_release", "-is"], capture_output=True, text=True).stdout.strip().lower()
            if distro in ["ubuntu", "debian"]:
                subprocess.run(["sudo", "apt", "install", "-y", "micro"], check=True)
            elif distro in ["fedora"]:
                subprocess.run(["sudo", "dnf", "install", "-y", "micro"], check=True)
            elif distro in ["arch", "manjaro"]:
                subprocess.run(["sudo", "pacman", "-S", "--noconfirm", "micro"], check=True)
            elif distro in ["opensuse"]:
                subprocess.run(["sudo", "zypper", "install", "-y", "micro-editor"], check=True)
            elif distro in ["gentoo"]:
                subprocess.run(["sudo", "emerge", "app-editors/micro"], check=True)
            elif distro in ["solus"]:
                subprocess.run(["sudo", "eopkg", "install", "-y", "micro"], check=True)
            else:
                console.print(f"[danger]Distro {distro} is not supported for automatic installation. Please install Micro manually.[/danger]")
                return

        elif os_type == "windows":
            subprocess.run(["choco", "install", "micro", "-y"], check=True) or \
            subprocess.run(["scoop", "install", "micro"], check=True) or \
            subprocess.run(["winget", "install", "zyedidia.micro"], check=True)

        elif os_type == "darwin":
            subprocess.run(["brew", "install", "micro"], check=True)

        elif os_type in ["openbsd", "netbsd", "freebsd"]:
            subprocess.run(["pkg_add", "-v", "micro"], check=True)

        console.print("[success]Micro editor was successfully installed![/success]")

    except subprocess.CalledProcessError as e:
        console.print(f"[danger]Failed to install Micro: {e}[/danger]")

def create_new_finscript_file():
    filename = input("Enter the filename for the new FinScript file: ") + ".fincept"
    if not os.path.exists(filename):
        with open(filename, "w") as file:
            console.print(f"[success]Created new file: {filename}[/success]")
            edit_finscript_file(filename)
    else:
        console.print(f"[danger]File {filename} already exists![/danger]")

def open_existing_finscript_file():
    filename = input("Enter the filename to open (with .fincept extension): ")
    if filename.endswith(".fincept") and os.path.exists(filename):
        edit_finscript_file(filename)
    else:
        console.print(f"[danger]File {filename} does not exist or has an invalid extension![/danger]")

def edit_finscript_file(filename):
    console.print(f"\n[info]Editing: {filename} with Micro editor[/info]")

    try:
        subprocess.run(["micro", filename], check=True)
    except subprocess.CalledProcessError:
        console.print(f"[bold red]Error: Micro editor failed to open.[/bold red]")
        return
    except FileNotFoundError:
        console.print(f"[bold red]Error: Micro editor not found. Please ensure Micro is installed.[/bold red]")
        return

    with open(filename, "r") as file:
        code_lines = file.read().splitlines()

    code_box = Panel(Syntax("\n".join(code_lines), "python", theme="monokai", line_numbers=True), title=f"Code in {filename}", style="bold green on #282828")
    console.print(code_box)

    run_finscript_file(filename)

def run_finscript_file(filename):
    console.print(f"[bold cyan]Running FinScript: {filename}[/bold cyan]\n", style="info")

    try:
        result = subprocess.run(
            ["finscript", "run", filename],
            capture_output=True,
            text=True,
            check=True
        )

        output_box = Panel(result.stdout.strip(), title="FinScript Output", style="bold blue on #282828")
        console.print(output_box)

    except subprocess.CalledProcessError as e:
        console.print(f"[bold red]Error executing the script: {e.stderr}[/bold red]")

def show_example_files():
    import webbrowser
    webbrowser.open("https://github.com/Fincept-Corporation/FinceptTerminal/tree/main/finscript/example")
    console.print("[bold green]Redirecting to FinScript Examples...[/bold green]")
    show_finscript_menu()


def show_finscript_docs():
    import webbrowser
    webbrowser.open("https://docs.fincept.in")
    console.print("[bold green]Redirecting to Fincept Documentation...[/bold green]")
    show_finscript_menu()

def show_finscript_help():
    console.print("[bold green]FinScript Help[/bold green]")
    console.print("[bold green]Create new files with .fincept extension and to run the script you need to execute - finscript run example.fincept || we are still working on proper docs[/bold green]")


