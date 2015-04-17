#include <sstream>
#include <iostream>
#include <string.h>

#include "hribf_buffers.h"

#define SMALLEST_CHUNK_SIZE 20 // Smallest possible size of a chunk in words
#define ACTUAL_BUFF_SIZE 8194 // HRIBF .ldf file format
#define OPTIMAL_CHUNK_SIZE 8187 // = 8194 - 2 (header size) - 2 (end of buffer) - 3 (spill chunk header)

const int end_spill_size = 20;
const int pacman_word1 = 2;
const int pacman_word2 = 9999;
const int buffend = -1;

BufferType::BufferType(int bufftype_, int buffsize_, int buffend_/*=-1*/){
	bufftype = bufftype_; buffsize = buffsize_; buffend = buffend_; zero = 0;
	debug_mode = false;
}

// Returns only false if not overwritten
bool BufferType::Write(std::ofstream *file_){
	return false;
}

DIR_buffer::DIR_buffer() : BufferType(542263620, 8192){
	total_buff_size = 8194;
	run_num = 0;
	unknown[0] = 0;
	unknown[1] = 1;
	unknown[2] = 2;
}
	
// DIR buffer (1 word buffer type, 1 word buffer size, 1 word for total buffer length,
// 1 word for total number of buffers, 2 unknown words, 1 word for run number, 1 unknown word,
// and 8186 zeros)
bool DIR_buffer::Write(std::ofstream *file_){
	if(!file_ || !file_->is_open()){ return false; }
	
	if(debug_mode){ std::cout << "debug: writing 32776 byte DIR buffer\n"; }
	
	file_->write((char*)&bufftype, 4);
	file_->write((char*)&buffsize, 4);
	file_->write((char*)&total_buff_size, 4);
	file_->write((char*)&zero, 4); // Will be overwritten later
	file_->write((char*)unknown, 8);
	file_->write((char*)&run_num, 4);
	file_->write((char*)&unknown[2], 4);
	
	// Fill the rest of the buffer with 0
	for(unsigned int i = 0; i < 8186; i++){
		file_->write((char*)&zero, 4);
	}
	
	return true;
}

void HEAD_buffer::set_char_array(std::string input_, char *arr_, unsigned int size_){
	for(unsigned int i = 0; i < size_; i++){
		arr_[i] = input_[i];
	}
}

HEAD_buffer::HEAD_buffer() : BufferType(1145128264, 64){ // 0x44414548 "HEAD"
	set_char_array("HHIRF   ", facility, 8);
	set_char_array("L003    ", format, 8);
	set_char_array("LIST DATA       ", type, 16);
	set_char_array("01/01/01 00:00  ", date, 16);
	run_num = 0;
}

bool HEAD_buffer::SetDateTime(){
	struct tm * local;
	time_t temp_time;
	time(&temp_time);
	local = localtime(&temp_time);
	
	int month = local->tm_mon+1;
	int day = local->tm_mday;
	int year = local->tm_year;
	int hour = local->tm_hour;
	int minute = local->tm_min;
	int second = local->tm_sec;
	
	std::stringstream stream;
	(month<10) ? stream << "0" << month << "/" : stream << month << "/";
	(day<10) ? stream << "0" << day << "/" : stream << day << "/";
	(year<110) ? stream << "0" << year-100 << " " : stream << year-100 << " ";
	
	(hour<10) ? stream << "0" << hour << ":" : stream << hour << ":";
	(minute<10) ? stream << "0" << minute << "  " : stream << minute << "  ";
	//stream << ":"
	//(second<10) ? stream << "0" << second : stream << second;
	
	std::string dtime_str = stream.str();
	if(dtime_str.size() > 16){ return false; }
	for(unsigned int i = 0; i < 16; i++){
		if(i >= dtime_str.size()){ date[i] = ' '; }
		else{ date[i] = dtime_str[i]; }
	}
	return true;
}
	
bool HEAD_buffer::SetTitle(std::string input_){
	for(unsigned int i = 0; i < 80; i++){
		if(i >= input_.size()){ run_title[i] = ' '; }
		else{ run_title[i] = input_[i]; }
	}
	return true;
}

// HEAD buffer (1 word buffer type, 1 word buffer size, 2 words for facility, 2 for format, 
// 3 for type, 1 word separator, 4 word date, 20 word title [80 character], 1 word run number,
// 30 words of padding, and 8129 end of buffer words)
bool HEAD_buffer::Write(std::ofstream *file_){
	if(!file_ || !file_->is_open()){ return false; }
	
	if(debug_mode){ std::cout << "debug: writing 32776 byte HEAD buffer\n"; }
	
	// write 140 bytes (35 words)
	file_->write((char*)&bufftype, 4);
	file_->write((char*)&buffsize, 4);
	file_->write(facility, 8);
	file_->write(format, 8);
	file_->write(type, 16);
	file_->write(date, 16);
	file_->write(run_title, 80);
	file_->write((char*)&run_num, 4);
		
	// Get the buffer length up to 256 bytes (add 29 words)
	char temp[116];
	for(unsigned int i = 0; i < 116; i++){ temp[i] = 0x0; }
	file_->write(temp, 116);
		
	// Fill the rest of the buffer with 0xFFFFFFFF (end of buffer)
	for(unsigned int i = 0; i < 8130; i++){
		file_->write((char*)&buffend, 4);
	}
	
	return true;
}

// Write data buffer header (2 words)
bool DATA_buffer::open_(std::ofstream *file_){
	if(!file_ || !file_->is_open()){ return false; }

	if(debug_mode){ std::cout << "debug: writing 2 word DATA header\n"; }
	file_->write((char*)&bufftype, 4); // write buffer header type
	file_->write((char*)&buffsize, 4); // write buffer size
	current_buff_pos = 2;
	buff_words_remaining = ACTUAL_BUFF_SIZE - 2;
	good_words_remaining = OPTIMAL_CHUNK_SIZE;

	return true;
}

DATA_buffer::DATA_buffer() : BufferType(1096040772, 8192){ // 0x41544144 "DATA"
	current_buff_pos = 0; 
	buff_words_remaining = ACTUAL_BUFF_SIZE;
	good_words_remaining = OPTIMAL_CHUNK_SIZE;
}

// Close a data buffer by padding with 0xFFFFFFFF
bool DATA_buffer::Close(std::ofstream *file_){
	if(!file_ || !file_->is_open()){ return false; }

	if(current_buff_pos < ACTUAL_BUFF_SIZE){
		if(debug_mode){ std::cout << "debug: closing buffer with " << ACTUAL_BUFF_SIZE - current_buff_pos << " 0xFFFFFFFF words\n"; }
		for(unsigned int i = current_buff_pos; i < ACTUAL_BUFF_SIZE; i++){
			file_->write((char*)&buffend, 4); 
		}
	}
	current_buff_pos = ACTUAL_BUFF_SIZE;
	buff_words_remaining = 0;
	good_words_remaining = 0;

	return true;
}

// Write data to file
bool DATA_buffer::Write(std::ofstream *file_, char *data_, unsigned int nWords_, int &buffs_written, int output_format_/*=0*/){
	if(!file_ || !file_->is_open() || !data_ || nWords_ == 0){ return false; }

	if(output_format_ == 0){ // legacy .ldf format
		// Write a DATA header if needed
		buffs_written = 0;
		if(current_buff_pos < 2){ open_(file_); }
		else if(current_buff_pos > ACTUAL_BUFF_SIZE){
			if(debug_mode){ std::cout << "debug: previous buffer overfilled by " << current_buff_pos - ACTUAL_BUFF_SIZE << " words!!!\n"; }
			Close(file_);
			open_(file_);
			buffs_written++;
		}
		else if(buff_words_remaining < SMALLEST_CHUNK_SIZE){ // Can't fit enough words in this buffer. Start a fresh one
			Close(file_);
			open_(file_);
			buffs_written++;
		}
	
		// The entire spill needs to be chopped up to fit into buffers
		// Calculate the number of data chunks we will need
		unsigned int words_written = 0;
		int this_chunk_sizeW, this_chunk_sizeB;
		int total_num_chunks, current_chunk_num;
		if((nWords_ + 10) >= good_words_remaining){ // Spill needs at least one more buffer	
			total_num_chunks = 2 + (nWords_ - good_words_remaining + 3) / OPTIMAL_CHUNK_SIZE;
			if((nWords_ - good_words_remaining + 3) % OPTIMAL_CHUNK_SIZE != 0){ 
				total_num_chunks++; // Account for the buffer fragment
			}
		}
		else{ // Entire spill (plus footer) will fit in the current buffer
			if(debug_mode){ std::cout << "debug: writing spill of nWords_=" << nWords_ << " + 10 words\n"; }
			
			// Write the spill chunk header
			this_chunk_sizeW = nWords_ + 3;
			this_chunk_sizeB = 4 * this_chunk_sizeW;
			total_num_chunks = 2; 
			current_chunk_num = 0;
			file_->write((char*)&this_chunk_sizeB, 4);
			file_->write((char*)&total_num_chunks, 4);
			file_->write((char*)&current_chunk_num, 4);
		
			// Write the spill
			file_->write((char*)data_, (this_chunk_sizeB - 12));
		
			// Write the end of spill buffer (5 words + 2 end of buffer words)
			current_chunk_num = 1;
			file_->write((char*)&buffend, 4);
			file_->write((char*)&end_spill_size, 4);
			file_->write((char*)&total_num_chunks, 4);
			file_->write((char*)&current_chunk_num, 4);
			file_->write((char*)&pacman_word1, 4);
			file_->write((char*)&pacman_word2, 4);
			file_->write((char*)&buffend, 4); // write 0xFFFFFFFF (signal end of spill footer)
		
			current_buff_pos += this_chunk_sizeW + 7;
			buff_words_remaining = ACTUAL_BUFF_SIZE - current_buff_pos;
				
			return true;
		} 

		if(debug_mode){
			std::cout << "debug: nWords_=" << nWords_ << ", total_num_chunks=" << total_num_chunks << ", current_buff_pos=" << current_buff_pos << std::endl;
			std::cout << "debug: buff_words_remaining=" << buff_words_remaining << ", good_words_remaining=" << good_words_remaining << std::endl;
		}
	
		current_chunk_num = 0;
		while(words_written < nWords_){
			// Calculate the size of this chunk
			if((nWords_ - words_written + 10) >= good_words_remaining){ // Spill chunk will require more than this buffer
				this_chunk_sizeW = good_words_remaining;
			
				// Write the chunk header
				this_chunk_sizeB = 4 * this_chunk_sizeW;
				file_->write((char*)&this_chunk_sizeB, 4);
				file_->write((char*)&total_num_chunks, 4);
				file_->write((char*)&current_chunk_num, 4);
		
				// Actually write the data
				if(debug_mode){ std::cout << "debug: writing spill chunk " << current_chunk_num << " of " << total_num_chunks << " with " << this_chunk_sizeW << " words\n"; }
				file_->write((char*)&data_[4*words_written], (this_chunk_sizeB - 12));
				file_->write((char*)&buffend, 4); // Mark the end of this chunk
				current_chunk_num++;
		
				current_buff_pos += this_chunk_sizeW + 1;
				buff_words_remaining = ACTUAL_BUFF_SIZE - current_buff_pos;		
				good_words_remaining = 0;
				words_written += this_chunk_sizeW - 3;
			
				Close(file_);
				open_(file_);
				buffs_written++;
			}
			else{ // Spill chunk (plus spill footer) will fit in this buffer. This is the final chunk
				this_chunk_sizeW = (nWords_ - words_written + 3);
			
				// Write the chunk header
				this_chunk_sizeB = 4 * this_chunk_sizeW;
				file_->write((char*)&this_chunk_sizeB, 4);
				file_->write((char*)&total_num_chunks, 4);
				file_->write((char*)&current_chunk_num, 4);
		
				// Actually write the data
				if(debug_mode){ std::cout << "debug: writing final spill chunk " << current_chunk_num << " with " << this_chunk_sizeW << " words\n"; }
				file_->write((char*)&data_[4*words_written], (this_chunk_sizeB - 12));
				file_->write((char*)&buffend, 4); // Mark the end of this chunk
				current_chunk_num++;

				current_buff_pos += this_chunk_sizeW + 1;
				buff_words_remaining = ACTUAL_BUFF_SIZE - current_buff_pos;		
				good_words_remaining = good_words_remaining - this_chunk_sizeW;
				words_written += this_chunk_sizeW - 3;
			}
		}

		// Can't fit spill footer. Fill with 0xFFFFFFFF and start new buffer instead
		if(good_words_remaining < 7){ 
			Close(file_);
			open_(file_);
			buffs_written++;
		}
	
		if(debug_mode){ std::cout << "debug: writing 24 bytes (6 words) for spill footer (chunk " << current_chunk_num << ")\n"; }
	
		// Write the end of spill buffer (5 words + 1 end of buffer words)
		file_->write((char*)&end_spill_size, 4);
		file_->write((char*)&total_num_chunks, 4);
		file_->write((char*)&current_chunk_num, 4);
		file_->write((char*)&pacman_word1, 4);
		file_->write((char*)&pacman_word2, 4);
		file_->write((char*)&buffend, 4); // write 0xFFFFFFFF (signal end of spill footer)
	
		current_buff_pos += 6;
		buff_words_remaining = ACTUAL_BUFF_SIZE - current_buff_pos;
		good_words_remaining = good_words_remaining - 6;
	
		if(debug_mode){ 
			std::cout << "debug: finished writing spill into " << buffs_written << " new buffers\n"; 
			if(total_num_chunks != current_chunk_num + 1){ 
				std::cout << "debug: total number of chunks does not equal number of chunks written (" << total_num_chunks << " != " << current_chunk_num+1 << ")!!!\n"; 
			}
			std::cout << std::endl;
		}

		return true;
	}
	else if(output_format_ == 1){
		if(debug_mode){ std::cout << "debug: .pld output format is not implemented!\n"; }
		return false;
	}
	else if(output_format_ == 2){
		if(debug_mode){ std::cout << "debug: .root output format is not implemented!\n"; }
		return false;
	}
	
	return false;
}

// EOF buffer (1 word buffer type, 1 word buffer size, and 8192 end of file words)
bool EOF_buffer::Write(std::ofstream *file_){
	if(!file_ || !file_->is_open()){ return false; }
	
	if(debug_mode){ std::cout << "debug: writing 32776 byte EOF buffer\n"; }
	
	// write 8 bytes (2 words)
	file_->write((char*)&bufftype, 4);
	file_->write((char*)&buffsize, 4);
	
	// Fill the rest of the buffer with 0xFFFFFFFF (end of buffer)
	for(unsigned int i = 0; i < 8192; i++){
		file_->write((char*)&buffend, 4);
	}
	
	return true;
}

/* Get the formatted filename of the current file. */
std::string PollOutputFile::get_filename(){
	std::stringstream stream; stream << current_file_num;
	std::string run_num_str = stream.str();
	std::string output;
	
	if(current_file_num == 0){ output = fname_prefix; }
	else if(current_file_num < 10){ output = fname_prefix + "0" + run_num_str; }
	else{ output = fname_prefix + run_num_str; }
	
	if(output_format == 0){ output += ".ldf"; }
	else if(output_format == 1){ output += ".pld"; }
	else{ output += ".root"; } // PLACEHOLDER!!!
	return output;
}

/// Overwrite the fourth word of the file with the total number of buffers and close the file
/// Returns false if no output file is open or if the number of 4 byte words in the file is not 
/// evenly divisible by the number of words in a buffer
bool PollOutputFile::overwrite_dir(int total_buffers_/*=-1*/){
	if(!output_file.is_open()){ return false; }
	
	// Set the buffer count in the "DIR " buffer
	if(total_buffers_ == -1){ // Set with the internal buffer count
		int size = output_file.tellp(); // Get the length of the file (in bytes)
		output_file.seekp(12); // Set position to just after the third word
	
		// Calculate the number of buffers in this file
		int total_num_buffer = size / (4 * ACTUAL_BUFF_SIZE);
		int overflow = size % (4 * ACTUAL_BUFF_SIZE);
		output_file.write((char*)&total_num_buffer, 4); 
		
		if(debug_mode){ 
			std::cout << "debug: file size is " << size << " bytes (" << size/4 << " 4 byte words)\n";
			std::cout << "debug: file contains " << total_num_buffer << " buffers of " << ACTUAL_BUFF_SIZE << " words\n";
			if(overflow != 0){ std::cout << "debug: file has an overflow of " << overflow << " 4 byte words!\n"; }
			std::cout << "debug: set .ldf directory buffer number to " << total_num_buffer << std::endl; 
		}
		
		if(overflow != 0){ 
			output_file.close();
			return false; 
		}
	} 
	else{ // Set with an external buffer count
		output_file.write((char*)&total_buffers_, 4); 
		if(debug_mode){ std::cout << "debug: set .ldf directory buffer number to " << total_buffers_ << std::endl; }	
	}
	
	output_file.close();
	return true;
}

PollOutputFile::PollOutputFile(){ 
	current_file_num = 0; 
	output_format = 0;
	number_spills = 0;
	fname_prefix = "poll_data";
	current_filename = "unknown";
	debug_mode = false;
}

PollOutputFile::PollOutputFile(std::string filename_){
	current_file_num = 0;
	output_format = 0;
	number_spills = 0;
	fname_prefix = filename_;
	current_filename = "unknown";
	debug_mode = false;
}

void PollOutputFile::SetDebugMode(bool debug_/*=true*/){
	debug_mode = debug_;
	dirBuff.SetDebugMode(debug_);
	headBuff.SetDebugMode(debug_);
	dataBuff.SetDebugMode(debug_);
	eofBuff.SetDebugMode(debug_);
}

bool PollOutputFile::SetFileFormat(int format_){
	if(format_ <= 2){
		output_format = format_;
		return true;
	}
	return false;
}

void PollOutputFile::SetFilenamePrefix(std::string filename_){ 
	fname_prefix = filename_; 
	current_file_num = 0;
}

int PollOutputFile::Write(char *data_, unsigned int nWords_){
	if(!data_ || nWords_ == 0){ return 0; }

	if(!output_file.is_open()){ return 0; }
	
	// Write data to disk
	int buffs_written;
	dataBuff.Write(&output_file, data_, nWords_, buffs_written, output_format);
	number_spills++;
	
	return buffs_written;
}

// Return the size of the packet to be built (in bytes)
unsigned int PollOutputFile::GetPacketSize(){
	if(!output_file.is_open()){ return (2 + 2 * sizeof(int)); }
	return ((2 + 4 * sizeof(int)) + sizeof(std::streampos) + current_filename.size());
}

/// Build a data spill notification message for broadcast onto the network
/// Return the total number of bytes in the packet upon success, and -1 otherwise
int PollOutputFile::BuildPacket(char *output){
	int end_packet = 0xFFFFFFFF;
	int buff_size = 8194;
	std::streampos file_size = output_file.tellp();

	int bytes = -1; // size of char array in bytes
	
	// Size of basic types on this machine. Probably overly cautious, but it only
	// amounts to sending two extra bytes over the network per packet
	char size_of_int = sizeof(int); // Size of integer on this machine
	char size_of_spos = sizeof(std::streampos); // Size of streampos on this machine

	if(!output_file.is_open()){
		// Below is the output packet structure
		// ------------------------------------
		// 1 byte size of integer (may not be the same on a different machine)
		// 1 byte size of streampos (may not be the same on a different machine)
		// 4 byte packet length (inclusive, also includes the end packet flag)
		// 4 byte begin packet flag (0xFFFFFFFF)
		bytes = 2 + 2 * sizeof(int); // Total size of the packet (in bytes)
		
		unsigned int index = 0;
		memcpy(&output[index], (char *)&size_of_int, 1); index += 1;
		memcpy(&output[index], (char *)&size_of_spos, 1); index += 1;
		memcpy(&output[index], (char *)&bytes, sizeof(int)); index += sizeof(int);
		memcpy(&output[index], (char *)&end_packet, sizeof(int)); index += sizeof(int);
	}
	else{
		// Below is the output packet structure
		// ------------------------------------
		// 1 byte size of integer (may not be the same on a different machine)
		// 1 byte size of streampos (may not be the same on a different machine)
		// 4 byte packet length (inclusive, also includes the end packet flag)
		// x byte file path (no size limit)
		// 8 byte file size streampos (long long)
		// 4 byte spill number ID (unsigned int)
		// 4 byte buffer size (unsigned int)
		// 4 byte begin packet flag (0xFFFFFFFF)
		// length of the file path.
		bytes = (2 + 4 * sizeof(int)) + sizeof(std::streampos) + current_filename.size(); // Total size of the packet (in bytes)
		const char *str = current_filename.c_str();
	
		unsigned int index = 0;
		memcpy(&output[index], (char *)&size_of_int, 1); index += 1;
		memcpy(&output[index], (char *)&size_of_spos, 1); index += 1;
		memcpy(&output[index], (char *)&bytes, sizeof(int)); index += sizeof(int);
		memcpy(&output[index], (char *)str, (size_t)current_filename.size()); index += current_filename.size();
		memcpy(&output[index], (char *)&file_size, sizeof(std::streampos)); index += sizeof(std::streampos);
		memcpy(&output[index], (char *)&number_spills, sizeof(int)); index += sizeof(int);
		memcpy(&output[index], (char *)&buff_size, sizeof(int)); index += sizeof(int);
		memcpy(&output[index], (char *)&end_packet, sizeof(int));
	}
	output[bytes] = '\0';
	
	return bytes;
}

// Close the current file, if one is open, and open a new file for data output
bool PollOutputFile::OpenNewFile(std::string title_, int run_num_, std::string &filename, std::string output_directory/*="./"*/){
	CloseFile();

	// Restart the spill counter for the new file
	number_spills = 0;

	std::ifstream dummy_file;
	filename = output_directory + get_filename();
	while(true){
		dummy_file.open(filename.c_str());
		if(dummy_file.is_open()){
			current_file_num++;
			filename = output_directory + get_filename();
			dummy_file.close();
		}
		else{
			output_file.open(filename.c_str(), std::ios::binary);
			if(!output_file.is_open()){
				output_file.close();
				return false;
			}
			
			current_filename = filename;			
			dirBuff.SetRunNumber(run_num_);
			dirBuff.Write(&output_file); // Every .ldf file gets a DIR header
			
			headBuff.SetTitle(title_);
			headBuff.SetDateTime();
			headBuff.SetRunNumber(run_num_);
			headBuff.Write(&output_file); // Every .ldf file gets a HEAD file header
				
			break;
		}
	}
	return true;
}

// Write the footer and close the file
void PollOutputFile::CloseFile(){
	if(!output_file.is_open()){ return; }
	
	dataBuff.Close(&output_file); // Pad the final data buffer with 0xFFFFFFFF
	
	eofBuff.Write(&output_file); // First EOF buffer signals end of run
	eofBuff.Write(&output_file); // Second EOF buffer signals physical end of file
	
	overwrite_dir(); // Overwrite the total buffer number word and close the file
}