#include <std_include.hpp>

#include "tool.hpp"

#include <utils/io.hpp>
#include <utils/flags.hpp>

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

		unsigned int little_endian(unsigned int be)
		{
			return  (be << 24) | ((be << 8) & 0x00FF0000) | ((be >> 8) & 0x0000FF00) | (be >> 24);
		}

		uint16_t little_endian(uint16_t be)
		{
			return ntohs(be);
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

			const auto header = little_endian(*reinterpret_cast<uint32_t*>(
				reinterpret_cast<uint64_t>(buffer)));

			const auto is_last = static_cast<bool>(header >> (8 * 3 + 7));
			const auto block_type = static_cast<block_type_t>((header << 1) >> (8 * 3 + 1));
			const auto block_length = (header << 8) >> 8;

			metadata_block_t block{};
			block.header.is_last = is_last;
			block.header.type = block_type;
			block.header.length = block_length;
			block.data = buffer + 4;

			return block;
		}

		void parse_metadata_block(char* buffer, metadata_block_t* block)
		{
			*block = parse_metadata_block(buffer);
		}

		bool verify_streaminfo_block(const metadata_block_t& block)
		{
			if (utils::flags::has_flag("-ignore-blocksize") || utils::flags::has_flag("i"))
			{
				return true;
			}

			const auto minimum = little_endian(*reinterpret_cast<uint16_t*>(block.data));
			const auto maximum = little_endian(*reinterpret_cast<uint16_t*>(block.data + 2));
			const auto res = (maximum == CONSTANT_BLOCKSIZE && minimum == CONSTANT_BLOCKSIZE);

			if (!res)
			{
				printf("Invalid streaminfo! Stream must have a constant blocksize of 1024 (was min: %i, max: %i)", minimum, maximum);
			}

			return res;
		}

		void convert_flac(const std::string& path)
		{
			std::string data{};
			if (!utils::io::read_file(path, &data))
			{
				printf("Failed to read file %s\n", path.data());
				return;
			}

			if (!check_signature(data))
			{
				printf("File is not a flac file\n");
				return;
			}

			const auto start_pos = data.data();
			auto pos = start_pos;
			pos += 4; // skip "fLaC"

			metadata_block_t block{};

			while (!block.header.is_last)
			{
				parse_metadata_block(pos, &block);

				if (!block.header.is_last)
				{
					pos = block.data + block.header.length;
				}

				if (block.header.type == block_type_t::application)
				{
					const auto application_id = std::string{block.data, 4};
					if (application_id == APPLICATION_ID)
					{
						printf("File has already been converted, aborting\n");
						return;
					}
				}

				if (block.header.type == block_type_t::streaminfo)
				{
					if (!verify_streaminfo_block(block))
					{
						return;
					}
				}
			}

			const auto insert_pos = static_cast<size_t>(pos - start_pos);
			auto frame_section_size = (start_pos + data.size()) - (block.data + block.header.length);
			auto header = 0x08000002; // application block, length 8

			std::string insert_data{};
			insert_data.append(reinterpret_cast<char*>(&header), 4); // header
			insert_data.append(APPLICATION_ID); // data (application id)
			insert_data.append(reinterpret_cast<char*>(&frame_section_size), 4); // data (frame section size)

			std::string new_data = data;
			new_data.insert(insert_pos, insert_data);

			const auto last_dot = path.find_last_of('.');
			const auto base_name = path.substr(0, last_dot);
			const auto new_name = base_name + "_converted.flac";

			utils::io::write_file(new_name, new_data, false);
			printf("Conversion successful!\n");
		}
	}

	void main(int argc, char** argv)
	{
		if (argc < 2)
		{
			printf("Usage: flac-tool <flac file>\n");
			return;
		}

		const auto path = argv[1];
		convert_flac(path);
	}
}
