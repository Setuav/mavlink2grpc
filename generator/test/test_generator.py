#!/usr/bin/env python3
"""
Test for core MAVLink dialects: minimal, standard, and common.
"""
import sys
import subprocess
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent))

from src.parser import MAVLinkParser
from src.generator import ProtoGenerator
from rich.console import Console
from rich.table import Table
from rich.panel import Panel
from rich.progress import Progress, SpinnerColumn, TextColumn, BarColumn

console = Console()

# Core dialects to test
CORE_DIALECTS = ["minimal", "standard", "common"]


def parse_all_dialects(parser: MAVLinkParser, dialect_names: list) -> dict:
    """Parse all dialect XML files."""
    dialects = {}
    failed = []

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        console=console,
    ) as progress:
        task = progress.add_task("Parsing dialects...", total=len(dialect_names))

        for name in dialect_names:
            try:
                dialect = parser.get_flattened_dialect(name)
                dialects[name] = dialect
                progress.update(task, advance=1)
            except Exception as e:
                console.print(f"[red][ERROR] Failed to parse {name}.xml: {e}[/red]")
                failed.append(name)
                progress.update(task, advance=1)

    return dialects, failed


def generate_all_protos(generator: ProtoGenerator, dialects: dict, output_dir: Path) -> dict:
    """Generate proto files for all dialects."""
    results = {"success": [], "failed": []}

    dialect_list = list(dialects.values())

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        console=console,
    ) as progress:
        task = progress.add_task("Generating protos...", total=None)

        try:
            generator.generate_all(dialect_list, output_dir)
            results["success"] = list(dialects.keys())
        except Exception as e:
            console.print(f"[red][ERROR] Generation failed: {e}[/red]")
            results["failed"] = list(dialects.keys())

        progress.update(task, completed=True)

    return results


def validate_proto(proto_file: Path, proto_dir: Path) -> tuple:
    """Validate a single proto file with protoc."""
    try:
        result = subprocess.run(
            ["protoc", f"--proto_path={proto_dir}", "--cpp_out=/tmp", str(proto_file)],
            capture_output=True,
            text=True,
            timeout=30,
        )

        if result.returncode == 0:
            # Check for warnings
            if result.stderr and "warning" in result.stderr.lower():
                return "warning", result.stderr
            return "valid", None
        else:
            return "error", result.stderr

    except subprocess.TimeoutExpired:
        return "error", "Validation timeout"
    except FileNotFoundError:
        return "error", "protoc not found"
    except Exception as e:
        return "error", str(e)


def validate_all_protos(proto_dir: Path, dialect_names: list) -> dict:
    """Validate all generated proto files."""
    results = {"valid": [], "warning": [], "error": []}

    with Progress(
        SpinnerColumn(),
        TextColumn("[progress.description]{task.description}"),
        BarColumn(),
        TextColumn("[progress.percentage]{task.percentage:>3.0f}%"),
        console=console,
    ) as progress:
        task = progress.add_task("Validating protos...", total=len(dialect_names))

        for name in dialect_names:
            proto_file = proto_dir / "mavlink" / f"{name}.proto"

            if not proto_file.exists():
                results["error"].append((name, "File not generated"))
                progress.update(task, advance=1)
                continue

            status, message = validate_proto(proto_file, proto_dir)
            results[status].append((name, message))
            progress.update(task, advance=1)

    return results


def main():
    """Main test function."""
    console.print(Panel.fit("Core MAVLink Dialects Test (minimal, standard, common)", style="bold blue"))

    # Paths
    xml_dir = Path(__file__).parent.parent.parent / "third_party" / "mavlink" / "message_definitions" / "v1.0"
    if not xml_dir.exists():
        xml_dir = Path(__file__).parent.parent.parent / "mavlink" / "message_definitions" / "v1.0"
    template_dir = Path(__file__).parent.parent / "templates"
    output_dir = Path(__file__).parent.parent.parent / "proto"

    if not xml_dir.exists():
        console.print(f"[red]Error: MAVLink definitions not found at {xml_dir}[/red]")
        return 1

    # Step 1: Use core dialects
    console.print("\n[cyan]Step 1: Testing core dialects...[/cyan]")
    dialect_names = CORE_DIALECTS
    console.print(f"[OK] Testing {len(dialect_names)} core dialects: {', '.join(dialect_names)}")

    # Step 2: Parse all dialects
    console.print("\n[cyan]Step 2: Parsing all dialects...[/cyan]")
    parser = MAVLinkParser(xml_dir)
    dialects, parse_failed = parse_all_dialects(parser, dialect_names)

    if parse_failed:
        console.print(f"[yellow]⚠ {len(parse_failed)} dialects failed to parse[/yellow]")
    console.print(f"[OK] Successfully parsed {len(dialects)}/{len(dialect_names)} dialects")

    # Step 3: Generate proto files
    console.print("\n[cyan]Step 3: Generating proto files...[/cyan]")
    generator = ProtoGenerator(template_dir, parser=parser)
    gen_results = generate_all_protos(generator, dialects, output_dir)

    if gen_results["failed"]:
        console.print(f"[red][ERROR] Failed to generate protos for {len(gen_results['failed'])} dialects[/red]")
    else:
        console.print(f"[OK] Generated protos for {len(gen_results['success'])} dialects")

    # Step 4: Validate with protoc
    console.print("\n[cyan]Step 4: Validating with protoc...[/cyan]")
    val_results = validate_all_protos(output_dir, list(dialects.keys()))

    # Summary table
    console.print("\n[bold cyan]Validation Results:[/bold cyan]")
    table = Table(title="Proto Validation Summary")
    table.add_column("Dialect", style="cyan")
    table.add_column("Enums", style="yellow", justify="right")
    table.add_column("Messages", style="yellow", justify="right")
    table.add_column("Proto File", style="white")
    table.add_column("Status", style="green")

    # Add valid files
    for name, _ in val_results["valid"]:
        if name in dialects:
            dialect = dialects[name]
            proto_file = output_dir / "mavlink" / f"{name}.proto"
            size = proto_file.stat().st_size if proto_file.exists() else 0
            table.add_row(
                name,
                str(len(dialect.enums)),
                str(len(dialect.messages)),
                f"{size:,} bytes",
                "[green]VALID[/green]"
            )

    # Add warnings
    for name, msg in val_results["warning"]:
        if name in dialects:
            dialect = dialects[name]
            proto_file = output_dir / "mavlink" / f"{name}.proto"
            size = proto_file.stat().st_size if proto_file.exists() else 0
            warning_line = msg.split('\n')[0] if msg else "Unknown warning"
            table.add_row(
                name,
                str(len(dialect.enums)),
                str(len(dialect.messages)),
                f"{size:,} bytes",
                f"[yellow]⚠ {warning_line[:30]}...[/yellow]"
            )

    # Add errors
    for name, msg in val_results["error"]:
        error_line = msg.split('\n')[0] if msg else "Unknown error"
        table.add_row(
            name,
            "-",
            "-",
            "-",
            f"[red][ERROR] {error_line[:30]}...[/red]"
        )

    console.print(table)

    # Final statistics
    console.print("\n[bold]Final Statistics:[/bold]")
    console.print(f"  Core dialects tested: {len(dialect_names)}")
    console.print(f"  Successfully parsed: {len(dialects)}")
    console.print(f"  Successfully generated: {len(gen_results['success'])}")
    console.print(f"  Valid protos: [green]{len(val_results['valid'])}[/green]")
    console.print(f"  Warnings: [yellow]{len(val_results['warning'])}[/yellow]")
    console.print(f"  Errors: [red]{len(val_results['error'])}[/red]")

    # Count total enums and messages
    total_enums = sum(len(d.enums) for d in dialects.values())
    total_messages = sum(len(d.messages) for d in dialects.values())
    console.print(f"\n  Total enums: {total_enums}")
    console.print(f"  Total messages: {total_messages}")

    # Success criteria (no errors allowed, warnings are ok)
    has_errors = len(val_results["error"]) > 0
    has_warnings = len(val_results["warning"]) > 0
    all_processed = (len(val_results["valid"]) + len(val_results["warning"])) == len(dialects)

    if not has_errors and all_processed and not has_warnings:
        console.print("\n[bold green]ALL CORE DIALECTS VALID![/bold green]")
        return 0
    elif not has_errors and all_processed and has_warnings:
        console.print("\n[bold yellow]All core dialects valid (with warnings)[/bold yellow]")
        return 0
    else:
        console.print("\n[bold red][ERROR] SOME CORE DIALECTS FAILED[/bold red]")
        return 1


if __name__ == "__main__":
    sys.exit(main())
