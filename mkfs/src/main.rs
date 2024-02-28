#![warn(rust_2018_idioms, unreachable_pub)]
#![forbid(elided_lifetimes_in_paths, unsafe_code)]

use anyhow::bail;
use bytemuck::{bytes_of, from_bytes_mut, NoUninit, Pod, Zeroable};
use clap::Parser;
use libc::{
	mode_t, S_IFDIR, S_IRGRP, S_IROTH, S_IRUSR, S_IWGRP, S_IWUSR, S_IXGRP,
	S_IXOTH, S_IXUSR
};
use std::{
	fs::{self, File},
	io::{self, Write},
	mem::size_of,
	num::NonZeroU32,
	os::linux::fs::MetadataExt,
	path::PathBuf
};

const OUICHEFS_MAGIC: &[u8; 4] = b"WICH";

const OUICHEFS_SB_BLOCK_NR: u32 = 0;

const OUICHEFS_BLOCK_SIZE: u32 = 1 << 12; /* 4 KiB */
const OUICHEFS_MAX_FILESIZE: usize = 1 << 22; /* 4 MiB */
const OUICHEFS_FILENAME_LEN: usize = 28;
const OUICHEFS_MAX_SUBFILES: usize = 128;

#[derive(Clone, Copy, Pod, Zeroable)]
#[repr(C)]
struct OuichefsInode {
	/// File mode
	i_mode: mode_t,
	/// Owner id
	i_uid: u32,
	/// Group id
	i_gid: u32,
	/// Size in bytes
	i_size: u32,
	/// Inode change time
	i_ctime: u32,
	/// Access time
	i_atime: u32,
	/// Modification time
	i_mtime: u32,
	/// Block count (subdir count for directories)
	i_blocks: u32,
	/// Hard links count
	i_nlink: u32,
	/// Block with list of blocks for this file
	index_block: u32
}

const OUICHEFS_INODES_PER_BLOCK: u32 =
	OUICHEFS_BLOCK_SIZE / size_of::<OuichefsInode>() as u32;

#[derive(Clone, Copy, NoUninit)]
#[repr(C)]
struct OuichefsSuperblock {
	/// Magic number
	magic: [u8; 4],

	/// Total number of blocks (incnl sb & inodes)
	nr_blocks: u32,
	/// Total number of inodes
	nr_inodes: u32,

	/// Number of inode store blocks
	nr_istore_blocks: u32,
	/// Number of free inodes bitmask blocks
	nr_ifree_blocks: u32,
	/// Number of free blocks bitmask blocks
	nr_bfree_blocks: u32,

	/// Number of free inodes
	nr_free_inodes: u32,
	/// Number of free blocks
	nr_free_blocks: u32,

	/// Padding to match block size
	_padding: [u8; 4064]
}

struct OuichefsFileIndexBlock {
	blocks: [u32; OUICHEFS_BLOCK_SIZE as usize >> 2]
}

struct OuichefsFile {
	inode: u32,
	filename: [u8; OUICHEFS_FILENAME_LEN]
}

struct OuichefsDirBlock {
	files: [OuichefsFile; OUICHEFS_MAX_SUBFILES]
}

fn write_superblock<W: Write>(
	mut w: W,
	stat: fs::Metadata
) -> io::Result<OuichefsSuperblock> {
	let nr_blocks = stat.st_size() as u32 / OUICHEFS_BLOCK_SIZE;
	let mut nr_inodes = nr_blocks;
	if let Some(rem) =
		NonZeroU32::new(nr_inodes % OUICHEFS_INODES_PER_BLOCK)
	{
		nr_inodes += rem.get();
	}
	let nr_istore_blocks = nr_inodes.div_ceil(OUICHEFS_INODES_PER_BLOCK);
	let nr_ifree_blocks = nr_inodes.div_ceil(OUICHEFS_BLOCK_SIZE * 8);
	let nr_bfree_blocks = nr_blocks.div_ceil(OUICHEFS_BLOCK_SIZE * 8);
	let nr_data_blocks = nr_blocks
		- 1 - nr_istore_blocks
		- nr_ifree_blocks
		- nr_bfree_blocks;

	let sb = OuichefsSuperblock {
		magic: *OUICHEFS_MAGIC,
		nr_blocks: nr_blocks.to_le(),
		nr_inodes: nr_inodes.to_le(),
		nr_istore_blocks: nr_istore_blocks.to_le(),
		nr_ifree_blocks: nr_ifree_blocks.to_le(),
		nr_bfree_blocks: nr_bfree_blocks.to_le(),
		nr_free_inodes: (nr_inodes - 1).to_le(),
		nr_free_blocks: (nr_data_blocks - 1).to_le(),
		_padding: [0u8; 4064]
	};

	w.write_all(bytes_of(&sb))?;

	println!("Superblock: ({})", size_of::<OuichefsSuperblock>());
	println!("\tmagic={:#x}", u32::from_ne_bytes(sb.magic));
	println!("\tnr_blocks={}", sb.nr_blocks);
	println!(
		"\tnr_inodes={} (istore={} blocks)",
		sb.nr_inodes, sb.nr_istore_blocks
	);
	println!("\tnr_ifree_blocks={}", sb.nr_ifree_blocks);
	println!("\tnr_bfree_blocks={}", sb.nr_bfree_blocks);
	println!("\tnr_free_inodes={}", sb.nr_free_inodes);
	println!("\tnr_free_blocks={}", sb.nr_free_blocks);
	Ok(sb)
}

fn write_inode_store<W: Write>(
	mut w: W,
	sb: &OuichefsSuperblock
) -> io::Result<()> {
	let mut block = [0u8; OUICHEFS_BLOCK_SIZE as usize];

	// Root inode (inode 0)
	let inode: &mut OuichefsInode =
		from_bytes_mut(&mut block[0..size_of::<OuichefsInode>()]);
	let first_data_block = 1
		+ u32::from_le(sb.nr_bfree_blocks)
		+ u32::from_le(sb.nr_ifree_blocks)
		+ u32::from_le(sb.nr_istore_blocks);
	inode.i_mode = (S_IFDIR
		| S_IRUSR | S_IRGRP
		| S_IROTH | S_IWUSR
		| S_IWGRP | S_IXUSR
		| S_IXGRP | S_IXOTH)
		.to_le();
	inode.i_uid = 0;
	inode.i_gid = 0;
	inode.i_size = OUICHEFS_BLOCK_SIZE.to_le();
	inode.i_ctime = 0;
	inode.i_atime = 0;
	inode.i_mtime = 0;
	inode.i_blocks = 1_u32.to_le();
	inode.i_nlink = 2_u32.to_le();
	inode.index_block = first_data_block.to_le();

	w.write_all(&block)?;

	// Reset inode store blocks to zero
	block = [0u8; OUICHEFS_BLOCK_SIZE as usize];
	for _ in 1..sb.nr_istore_blocks {
		w.write_all(&block)?;
	}

	println!("Inode store: wrote {} blocks", sb.nr_istore_blocks);
	println!("\tinode size = {} B", size_of::<OuichefsInode>());
	Ok(())
}

fn write_ifree_blocks<W: Write>(
	mut w: W,
	sb: &OuichefsSuperblock
) -> io::Result<()> {
	let mut ifree =
		[u64::MAX; OUICHEFS_BLOCK_SIZE as usize / size_of::<u64>()];

	// First ifree block, containing first used inode
	ifree[0] = (u64::MAX - 1).to_le();
	w.write_all(bytes_of(&ifree))?;

	// All ifree blocks except the one containing 2 first inodes
	ifree[0] = u64::MAX;
	for _ in 1..sb.nr_ifree_blocks {
		w.write_all(bytes_of(&ifree))?;
	}

	println!("Ifree blocks: wrote {} blocks", sb.nr_ifree_blocks);
	Ok(())
}

fn write_bfree_blocks<W: Write>(
	mut w: W,
	sb: &OuichefsSuperblock
) -> io::Result<()> {
	let mut bfree =
		[u64::MAX; OUICHEFS_BLOCK_SIZE as usize / size_of::<u64>()];

	// First blocks (incl. sb + istore + ifree + bfree + 1 used block)
	// we suppose it won't go further than the first block
	let mut nr_used = u32::from_le(sb.nr_istore_blocks)
		+ u32::from_le(sb.nr_ifree_blocks)
		+ u32::from_le(sb.nr_bfree_blocks)
		+ 2;
	let mut i = 0;
	while nr_used != 0 {
		let mut line = u64::MAX;
		let mut mask = 1;
		while mask != 0 {
			line &= !mask;
			nr_used -= 1;
			if nr_used == 0 {
				break;
			}
			mask <<= 1;
		}
		bfree[i] = line.to_le();
		i += 1;
	}
	w.write_all(bytes_of(&bfree))?;

	// other blocks
	bfree = [u64::MAX; OUICHEFS_BLOCK_SIZE as usize / size_of::<u64>()];
	for _ in 1..sb.nr_bfree_blocks {
		w.write_all(bytes_of(&bfree))?;
	}

	println!("Bfree blocks: wrote {} blocks", sb.nr_bfree_blocks);
	Ok(())
}

fn main() -> anyhow::Result<()> {
	#[derive(Parser)]
	struct Args {
		device: PathBuf
	}

	let args = Args::parse();

	// Open disk image
	let mut file = File::options().write(true).open(&args.device)?;
	// Get image size
	let stat = args.device.metadata()?;

	// Check if image is large enough
	let min_size = 100 * OUICHEFS_BLOCK_SIZE as u64;
	if stat.st_size() < min_size {
		bail!("File is not large enough (size={}, min size={min_size})", stat.st_size());
	}

	// Write superblock (block 0)
	let sb = write_superblock(&mut file, stat)?;

	// Write inode store blocks
	write_inode_store(&mut file, &sb)?;

	// Write inode free bitmap blocks
	write_ifree_blocks(&mut file, &sb)?;

	// Write block free bitmap blocks
	write_bfree_blocks(&mut file, &sb)?;

	Ok(())
}
