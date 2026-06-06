#!/usr/bin/env python3
"""
MAVLink to Protocol Buffer Generator - CLI Tool
"""
import sys
import argparse
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from src.parser import MAVLinkParser
from src.generator import ProtoGenerator
from rich.console import Console
from rich.panel import Panel

console = Console()


def parse_args():
    parser = argparse.ArgumentParser(description="MAVLink to Protocol Buffer Generator")
    parser.add_argument(
        "--dialect",
        type=str,
        default="common",
        choices=["minimal", "standard", "common"],
        help="MAVLink dialect to generate (default: common)"
    )
    parser.add_argument(
        "--proto-out",
        type=str,
        help="Output directory for generated proto files"
    )
    parser.add_argument(
        "--cpp-out",
        type=str,
        help="Output directory for generated C++ converter files"
    )
    parser.add_argument(
        "--xml-dir",
        type=str,
        help="Directory containing MAVLink XML definitions"
    )
    return parser.parse_args()


def main():
    args = parse_args()
    selected_dialect = args.dialect

    # Header
    console.print(Panel.fit(
        "[bold cyan]MAVLink to Protocol Buffer Generator[/bold cyan]",
        border_style="cyan"
    ))
    console.print(f"[cyan]Selected dialect:[/cyan] {selected_dialect}\n")

    # Setup paths
    base_dir = Path(__file__).parent.parent
    xml_dir = Path(args.xml_dir) if args.xml_dir else base_dir / "third_party" / "mavlink" / "message_definitions" / "v1.0"
    output_dir = Path(args.proto_out) if args.proto_out else base_dir / "proto"
    converter_output = Path(args.cpp_out) if args.cpp_out else base_dir / "bridge" / "src" / "mavlink"
    template_dir = Path(__file__).parent / "templates"

    # Create output directories
    output_dir.mkdir(parents=True, exist_ok=True)
    converter_output.mkdir(parents=True, exist_ok=True)

    # Parse dialect (with includes merged)
    console.print("[bold]Parsing dialect (including dependencies)...[/bold]")
    mavlink_parser = MAVLinkParser(xml_dir)

    try:
        dialect = mavlink_parser.get_flattened_dialect(selected_dialect)
        console.print(f"  [green][OK][/green] {selected_dialect}: {len(dialect.enums)} enums, {len(dialect.messages)} messages")
    except Exception as e:
        console.print(f"  [red][ERROR][/red] {selected_dialect}: ERROR - {e}")
        return 1

    # Generate proto file
    console.print(f"\n[bold]Generating proto file...[/bold]")
    proto_gen = ProtoGenerator(template_dir, mavlink_parser)

    try:
        proto_gen.generate_all([dialect], output_dir)
        console.print(f"  [green][OK][/green] Generated {selected_dialect}.proto")
        console.print(f"  [green][OK][/green] Generated bridge service proto")
    except Exception as e:
        console.print(f"  [red][ERROR][/red] ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

    # Generate C++ message converter
    console.print(f"\n[bold]Generating C++ MessageConverter...[/bold]")
    
    try:
        proto_gen.generate_message_converter(
            {selected_dialect: dialect},
            converter_output
        )
        console.print(f"  [green][OK][/green] Generated MessageConverter.h")
        console.print(f"  [green][OK][/green] Generated MessageConverter.cc")
    except Exception as e:
        console.print(f"  [red][ERROR][/red] ERROR: {e}")
        import traceback
        traceback.print_exc()
        return 1

    # Summary
    total_enums = len(dialect.enums)
    total_messages = len(dialect.messages)

    console.print()
    console.print(Panel.fit(
        f"[bold green]Successfully completed![/bold green]\n\n"
        f"Output directory: [cyan]{output_dir}[/cyan]\n"
        f"Total: [yellow]{total_enums}[/yellow] enums, [yellow]{total_messages}[/yellow] messages",
        border_style="green"
    ))

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        console.print("\n\n[yellow]Operation cancelled.[/yellow]")
        sys.exit(1)
