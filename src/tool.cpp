#include <std_include.hpp>

#include "tool.hpp"

#include <utils/io.hpp>
#include <utils/flags.hpp>
#include <utils/string.hpp>

#define APPLICATION_ID "fsiz"
#define CONSTANT_BLOCKSIZE 0x400

namespace tool
{
	namespace
	{
		enum block_type_t
		{
			streaminfo,
			padding,
			application,
			seektable,
			vorbis_comment,
			cuesheet,
			picture,
			count
		};

		struct metadata_block_header_t
		{
			bool is_last;
			block_type_t type;
			int length;
		};

		struct metadata_block_t
		{
			metadata_block_header_t header;
			char* data;
			char* start;
		};

		bool check_signature(const std::string& buffer)
		{
			static std::vector<uint8_t> signature = {'f', 'L', 'a', 'C'};

			if (buffer.size() <= signature.size())
			{
				return false;
			}

			for (auto i = 0; i < signature.size(); i++)
			{
				if (buffer[i] != signature[i])
				{
					return false;
				}
			}

			return true;
		}

		metadata_block_t parse_metadata_block(char* buffer)
		{
			/*
				// https://xiph.org/flac/format.html#metadata_block_header

				bits | description
				   1   is last block
				   7   block type
				  24   block length (header not included)
			*/

			const auto header = _byteswap_ulong(*reinterpret_cast<uint32_t*>(
				reinterpret_cast<uint64_t>(buffer)));

			const auto is_last = static_cast<bool>(header >> (8 * 3 + 7));
			const auto block_type = static_cast<block_type_t>((header << 1) >> (8 * 3 + 1));
			const auto block_length = (header << 8) >> 8;

			metadata_block_t block{};
			block.header.is_last = is_last;
			block.header.type = block_type;
			block.header.length = block_length;
			block.data = buffer + 4;
			block.start = buffer;

#ifdef DEBUG
			printf("Found block of type %i\n", block.header.type);
#endif

			return block;
		}

		void parse_metadata_block(char* buffer, metadata_block_t* block)
		{
			*block = parse_metadata_block(buffer);
		}

		void verify_streaminfo_block(const metadata_block_t& block)
		{
			if (utils::flags::has_flag("-ignore-blocksize") || utils::flags::has_flag("i"))
			{
				return;
			}

			const auto minimum = _byteswap_ushort(*reinterpret_cast<uint16_t*>(block.data));
			const auto maximum = _byteswap_ushort(*reinterpret_cast<uint16_t*>(block.data + 2));

			if (maximum != CONSTANT_BLOCKSIZE || minimum != CONSTANT_BLOCKSIZE)
			{
				throw std::runtime_error(utils::string::va(
					"Stream must have a constant blocksize of 1024! (was min: %i, max: %i)", 
						minimum, maximum));
			}
		}

		void convert_flac(const std::string& path, const std::optional<std::string>& out_path)
		{
			std::string data{};
			if (!utils::io::read_file(path, &data))
			{
				throw std::runtime_error("Failed to read file " + path);
			}

			if (!check_signature(data))
			{
				throw std::runtime_error("File is not a flac file");
			}

			const auto start_pos = data.data();
			auto pos = start_pos;
			pos += 4; // skip "fLaC"

			metadata_block_t block{};

			auto num_blocks = 0;
			bool has_seektable = false;

			while (!block.header.is_last)
			{
				parse_metadata_block(pos, &block);
				num_blocks++;

				if (!block.header.is_last)
				{
					pos = block.data + block.header.length;
				}

				if (block.header.type == block_type_t::application)
				{
					const auto application_id = std::string{block.data, 4};
					if (application_id == APPLICATION_ID)
					{
						throw std::runtime_error("File has already been converted, aborting\n");
					}
				}

				if (block.header.type == block_type_t::streaminfo)
				{
					verify_streaminfo_block(block);
				}

				if (block.header.type == block_type_t::seektable)
				{
					has_seektable = true;
				}
			}

			const auto frame_section_size = (start_pos + data.size()) - (block.data + block.header.length);
			auto insert_pos = static_cast<size_t>(pos - start_pos);
			auto insert_header = _byteswap_ulong(0x02000008); // application block, length 8

			if (num_blocks == 1)
			{
				const auto header_ptr = reinterpret_cast<uint32_t*>(block.start);
				const auto header = _byteswap_ulong(*header_ptr);
				*header_ptr = _byteswap_ulong((header << 1) >> 1);

				insert_header = _byteswap_ulong(0x82000008);
				insert_pos = static_cast<size_t>(block.data - start_pos + block.header.length);
			}

			std::string insert_data{};

			if (!has_seektable)
			{
				printf("[Warning] Seektable not found! Adding empty seektable\n");
				const auto seektable_ = _byteswap_ulong(0x03000000);
				insert_data.append(reinterpret_cast<const char*>(&seektable_), 4);
			}

			insert_data.append(reinterpret_cast<const char*>(&insert_header), 4); // header
			insert_data.append(APPLICATION_ID); // data (application id)
			insert_data.append(reinterpret_cast<const char*>(&frame_section_size), 4); // data (frame section size)

			std::string new_data = data;
			new_data.insert(insert_pos, insert_data);
			
			std::string new_name{};
			if (out_path.has_value())
			{
				new_name = out_path.value();
			}
			else
			{
				const auto last_dot = path.find_last_of('.');
				const auto base_name = path.substr(0, last_dot);
				new_name = base_name + "_converted.flac";
			}

			utils::io::write_file(new_name, new_data, false);
			printf("Conversion successful!\nSaved to %s\n", new_name.data());
		}

		void start_unsafe(int argc, char** argv)
		{
			if (argc < 2)
			{
				printf("Usage: flac-tool <flac file>\n");
				return;
			}

			const auto path = argv[1];

			auto output_path = utils::flags::get_flag("o");
			if (!output_path.has_value())
			{
				output_path = utils::flags::get_flag("-output");
			}

			convert_flac(path, output_path);
		}
	}

	void main(int argc, char** argv)
	{
		try
		{
			start_unsafe(argc, argv);
		}
		catch (const std::exception& e)
		{
			printf("Conversion failed: %s\n", e.what());
		}
	}
}
