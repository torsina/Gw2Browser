/* \file       Viewers/SoundPlayer/SoundPlayer.cpp
*  \brief      Contains definition of the sound player.
*  \author     Khral Steelforge
*/

/*
Copyright (C) 2016-2017 Khral Steelforge <https://github.com/kytulendu>

This file is part of Gw2Browser.

Gw2Browser is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "stdafx.h"

#include "SoundPlayer.h"
#include "Exception.h"
#include "EventId.h"

#include "Data.h"

namespace gw2b {

	SoundPlayer::SoundPlayer( wxWindow* p_parent, const wxPoint& p_pos, const wxSize& p_size )
		: Viewer( p_parent, p_pos, p_size ) {

		// Initialize OpenAL
		if ( !m_alInitialized ) {
			if ( !this->initOAL( ) ) {
				throw exception::Exception( "Could not initialize OpenAL." );
			}
			m_alInitialized = true;
		}

		auto sizer = new wxBoxSizer( wxVERTICAL );
		//auto hsizer1 = new wxBoxSizer( wxHORIZONTAL );
		auto hsizer2 = new wxBoxSizer( wxHORIZONTAL );

		// List control
		m_listCtrl = new wxListCtrl( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_THEME );
		sizer->Add( m_listCtrl, wxSizerFlags( ).Expand( ).Proportion( 1 ) );

		// Playback slider
		//m_slider = new wxSlider( this, ID_SliderPlayback, 0, 0, 1, wxDefaultPosition, wxSize( 400, 25) );
		//hsizer1->Add( m_slider );
		//sizer->Add( hsizer1 );

		// Sound control
		auto back = new wxButton( this, ID_BtnBack, wxT( "|<" ) );
		auto play = new wxButton( this, ID_BtnPlay, wxT( ">" ) );
		auto stop = new wxButton( this, ID_BtnStop, wxT( "[]" ) );
		auto forward = new wxButton( this, ID_BtnForward, wxT( ">|" ) );
		m_volSlider = new wxSlider( this, ID_SliderVolume, 100, 0, 100, wxDefaultPosition, wxSize( 100, 25 ) );
		hsizer2->Add( back );
		hsizer2->Add( play );
		hsizer2->Add( stop );
		hsizer2->Add( forward );
		hsizer2->Add( m_volSlider );
		sizer->Add( hsizer2 );

		// Layout
		this->SetSizer( sizer );
		this->Layout( );

		// List control events
		this->Bind( wxEVT_LIST_ITEM_ACTIVATED, &SoundPlayer::onListItemDoubleClickedEvt, this );
		// Button events
		this->Bind( wxEVT_BUTTON, &SoundPlayer::onButtonEvt, this );
		// Volume events
		this->Bind( wxEVT_SLIDER, &SoundPlayer::onVolChange, this );
	}

	SoundPlayer::~SoundPlayer( ) {
		this->stopSound( );

		// Delete buffers
		alDeleteBuffers( NUM_BUFFERS, m_buffers );

		// Close OpenAL
		alcMakeContextCurrent( NULL );
		alcDestroyContext( m_context );
		alcCloseDevice( m_device );
	}

	void SoundPlayer::clear( ) {
		this->stopSound( );

		// Clear list control content
		m_listCtrl->ClearAll( );

		m_sound.clear( );
		Viewer::clear( );
	}

	void SoundPlayer::setReader( FileReader* p_reader ) {
		if ( !m_alInitialized ) {
			// Could not initialize OpenAL
			return;
		}

		Viewer::setReader( p_reader );
		if ( p_reader ) {
			if ( isOfType<SoundBankReader>( p_reader ) ) {
				if ( p_reader ) {
					auto reader = this->sndBankReader( );
					m_sound = reader->getSoundData( );
				}
			} else if ( isOfType<PackedSoundReader>( p_reader ) ) {
				if ( p_reader ) {
					auto reader = this->pfSoundReader( );
					auto sound = reader->getSoundData( );

					SoundBank tmp;
					tmp.voiceId = 0;
					tmp.data = sound;

					m_sound.push_back( tmp );
				}
			} else if ( isOfType<asndMP3Reader>( p_reader ) ) {
				if ( p_reader ) {
					auto reader = this->asndSoundReader( );
					auto sound = reader->getMP3Data( );

					SoundBank tmp;
					tmp.voiceId = 0;
					tmp.data = sound;

					m_sound.push_back( tmp );
				}
			}
			// Populate the list control with data list
			this->populateListCtrl( );

			if ( !m_sound.empty( ) ) {
				// Select the first entry
				this->selectEntry( 0 );
				//Play the sound
				this->playSound( 0 );
			}
		}
	}

	void SoundPlayer::printDeviceList( const char *p_list ) {
		if ( !p_list || *p_list == '\0' ) {
			printf( "    !!! none !!!\n" );
			wxLogMessage( wxT( "    !!! NONE !!!" ) );
		} else {
			do {
				wxLogMessage( wxT( "    %s" ), p_list );
				p_list += strlen( p_list ) + 1;
			} while ( *p_list != '\0' );
		}
	}

	bool SoundPlayer::initOAL( ) {
		wxLogMessage( wxT( "Initializing OpenAL..." ) );

		if ( !alcIsExtensionPresent( NULL, "ALC_ENUMERATION_EXT" ) ) {
			wxLogMessage( wxT( "Enumeration extension not available." ) );
		}

		wxLogMessage( wxT( "Available playback devices:" ) );
		if ( !alcIsExtensionPresent( NULL, "ALC_ENUMERATE_ALL_EXT" ) ) {
			this->printDeviceList( alcGetString( NULL, ALC_ALL_DEVICES_SPECIFIER ) );
		} else {
			this->printDeviceList( alcGetString( NULL, ALC_DEVICE_SPECIFIER ) );
		}

		if ( !alcIsExtensionPresent( NULL, "ALC_ENUMERATE_ALL_EXT" ) ) {
			wxLogMessage( wxT( "Default playback device: %s" ), alcGetString( NULL, ALC_DEFAULT_ALL_DEVICES_SPECIFIER ) );
		} else {
			wxLogMessage( wxT( "Default playback device: %s" ), alcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER ) );
		}

		const ALCchar* defaultDeviceName = alcGetString( NULL, ALC_DEFAULT_DEVICE_SPECIFIER );

		m_device = alcOpenDevice( defaultDeviceName );
		if ( !m_device ) {
			wxLogMessage( wxT( "Unable to open default device." ) );
			return false;
		}

		// Clear error code
		alGetError( );

		m_context = alcCreateContext( m_device, NULL );
		if ( !alcMakeContextCurrent( m_context ) ) {
			alcCloseDevice( m_device );
			wxLogMessage( wxT( "Failed to make default context." ) );
			return false;
		}

		wxLogMessage( wxT( "OpenAL vendor string: %s" ), alGetString( AL_VENDOR ) );
		wxLogMessage( wxT( "OpenAL renderer string: %s" ), alGetString( AL_RENDERER ) );
		wxLogMessage( wxT( "OpenAL version string: %s" ), alGetString( AL_VERSION ) );

		// Generate buffers
		alGenBuffers( NUM_BUFFERS, m_buffers );

		return true;
	}

	void SoundPlayer::populateListCtrl( ) {
		wxListItem itemCol;
		itemCol.SetText( wxT( "" ) );
		m_listCtrl->InsertColumn( 0, itemCol );

		itemCol.SetText( wxT( "Entry" ) );
		itemCol.SetAlign( wxLIST_FORMAT_CENTRE );
		m_listCtrl->InsertColumn( 1, itemCol );

		//itemCol.SetText( wxT( "Lenght" ) );
		//itemCol.SetAlign( wxLIST_FORMAT_LEFT );
		//m_listCtrl->InsertColumn( 2, itemCol );

		// to speed up inserting we hide the control temporarily
		m_listCtrl->Hide( );

		for ( size_t i = 0; i < m_sound.size( ); i++ ) {
			this->insertItem( i );
		}

		m_listCtrl->Show( );

		m_listCtrl->SetColumnWidth( 0, 20 );
		m_listCtrl->SetColumnWidth( 1, 100 );
		//m_listCtrl->SetColumnWidth( 2, wxLIST_AUTOSIZE );

	}

	void SoundPlayer::insertItem( const int p_index ) {
		wxString buf;
		buf.Printf( wxT( "*" ) );
		long tmp = m_listCtrl->InsertItem( p_index, buf, 0 );
		m_listCtrl->SetItemData( tmp, p_index );

		buf.Printf( wxT( "%u" ), m_sound[p_index].voiceId );
		m_listCtrl->SetItem( tmp, 1, buf );

		//buf.Printf( wxT( "Item %d" ), p_index );
		//m_listCtrl->SetItem( tmp, 2, buf );
	}

	void SoundPlayer::selectEntry( const long p_index ) {
		m_listCtrl->SetItemState( p_index, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED );

		auto sel = m_listCtrl->GetNextItem( -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED );
		if ( sel != -1 ) {
			m_listCtrl->SetItemState( sel, 0, wxLIST_STATE_SELECTED );
		}
		m_listCtrl->SetItemState( p_index, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED );
	}

	bool SoundPlayer::loadOggs( char* p_data, const size_t p_size, OggVorbis_File* p_oggFile, ogg_file& p_oggStream, ov_callbacks& p_oggCallbacks ) {
		p_oggStream.curPtr = p_oggStream.filePtr = p_data;
		p_oggStream.fileSize = p_size;

		p_oggCallbacks.read_func = readOgg;
		p_oggCallbacks.seek_func = seekOgg;
		p_oggCallbacks.close_func = closeOgg;
		p_oggCallbacks.tell_func = tellOgg;

		auto ret = ov_open_callbacks( static_cast<void*>( &p_oggStream ), p_oggFile, NULL, -1, p_oggCallbacks );
		if ( ret ) {
			wxLogMessage( wxT( "ov_open_callbacks error :(" ) );
			switch ( ret ) {
			case OV_EREAD:
				wxLogMessage( wxT( "A read from media returned an error." ) );
				break;
			case OV_ENOTVORBIS:
				wxLogMessage( wxT( "Bitstream does not contain any Vorbis data." ) );
				break;
			case OV_EVERSION:
				wxLogMessage( wxT( "Vorbis version mismatch." ) );
				break;
			case OV_EBADHEADER:
				wxLogMessage( wxT( "Invalid Vorbis bitstream header." ) );
				break;
			case OV_EFAULT:
				wxLogMessage( wxT( "Internal logic fault; indicates a bug or heap/stack corruption." ) );
				break;
			default:
				wxLogMessage( wxT( "Unknown error." ) );
			}
			return false;
		}

		// Get some information about the OGG file
		vorbis_info* oggInfo = ov_info( p_oggFile, -1 );
		if ( !oggInfo ) {
			ov_clear( p_oggFile );
			wxLogMessage( wxT( "Can't get vorbis information :(" ) );
			return false;
		}

		vorbis_comment* oggComments = ov_comment( p_oggFile, -1 );
		char **ptr = oggComments->user_comments;
		while ( *ptr ) {
			wxLogMessage( wxT( "%s" ), *ptr );
			++ptr;
		}
		wxLogMessage( wxT( "Bitstream is Ogg Vorbis, %d channel, %ld Hz" ), oggInfo->channels, oggInfo->rate );
		wxLogMessage( wxT( "Bitrate (variable): %ld kb/s" ), oggInfo->bitrate_nominal / 1000 );
		wxLogMessage( wxT( "Decoded length: %ld samples" ), static_cast<long>( ov_pcm_total( p_oggFile, -1 ) ) );
		wxLogMessage( wxT( "Encoded by: %s" ), oggComments->vendor );

		// Check the number of channels
		if ( oggInfo->channels == 1 ) {
			m_format = AL_FORMAT_MONO16;
		} else {
			m_format = AL_FORMAT_STEREO16;
		}

		// The frequency of the sampling rate
		m_frequency = oggInfo->rate;

		return true;
	}

	bool SoundPlayer::readOggs( char* p_data, const ALsizei p_count, OggVorbis_File* p_oggFile, ALuint p_buffer ) {
		size_t size = p_count * sizeof( ALshort );
		size_t bytes = 0;
		while ( bytes < size ) {
			auto read = ov_read( p_oggFile, p_data + bytes, size - bytes, 0, 2, 1, 0 );
			if ( read > 0 ) {
				bytes += read;
			} else {
				break;
			}
		}

		if ( bytes == 0 ) {
			return false;
		}

		// Buffer decoded sound data to OpenAL buffer
		alBufferData( p_buffer, m_format, p_data, bytes, m_frequency );
		//wxLogMessage( wxT( "Read %d bytes" ), bytes / 2 );
		return true;
	}

	bool SoundPlayer::loadMp3( char* p_data, const size_t p_size, mpg123_handle* p_handle ) {
		auto data = reinterpret_cast<unsigned char*>( p_data );

		long sampleRate;
		int channels;
		int encoding;

		if ( mpg123_open_feed( p_handle ) != MPG123_OK ) {
			mpg123_delete( p_handle );
			return false;
		}

		if ( mpg123_feed( p_handle, data, p_size ) != MPG123_OK ) {
			mpg123_delete( p_handle );
			return false;
		}

		if ( mpg123_getformat( p_handle, &sampleRate, &channels, &encoding ) != MPG123_OK ) {
			mpg123_close( p_handle );
			mpg123_delete( p_handle );
			return false;
		}

		if ( ( channels == 1 || channels == 2 ) && sampleRate > 0 ) {
			if ( mpg123_format_none( p_handle ) == MPG123_OK ) {
				if ( mpg123_format( p_handle, sampleRate, channels, MPG123_ENC_SIGNED_16 ) == MPG123_OK ) {
					// Set sample rate
					m_frequency = sampleRate;
					// set number of channels
					if ( channels == 1 ) {
						m_format = AL_FORMAT_MONO16;
					} else {
						m_format = AL_FORMAT_STEREO16;
					}

					mpg123_scan( p_handle );

					// Get some information about the Mp3 file
					mpg123_frameinfo info;
					mpg123_info( p_handle, &info );

					wxString versionString;
					switch ( info.version ) {
					case MPG123_1_0:
						versionString = wxT( "MPEG Version 1.0" );
						break;
					case MPG123_2_0:
						versionString = wxT( "MPEG Version 2.0" );
						break;
					case MPG123_2_5:
						versionString = wxT( "MPEG Version 2.5" );
						break;
					}

					wxString channelString;
					switch ( info.mode ) {
					case MPG123_M_STEREO:
						channelString = wxT( "Standard Stereo" );
						break;
					case MPG123_M_JOINT:
						channelString = wxT( "Joint Stereo" );
						break;
					case MPG123_M_DUAL:
						channelString = wxT( "Dual Channel" );
						break;
					case MPG123_M_MONO:
						channelString = wxT( "Single Channel" );
						break;
					}

					wxLogMessage( wxT( "Bitstream is %s Layer %d, %s, %ld Hz " ), versionString, info.layer, channelString, info.rate );

					wxString modeString;
					switch ( info.vbr ) {
					case MPG123_CBR:
						modeString = wxT( "Constant Bitrate" );
						break;
					case MPG123_VBR:
						modeString = wxT( "Variable Bitrate" );
						break;
					case MPG123_ABR:
						modeString = wxT( "Average Bitrate" );
						break;
					}
					wxLogMessage( wxT( "Bitrate: %s mode, %d kb/s" ), modeString, info.bitrate );
					wxLogMessage( wxT( "Decoded length: %ld samples" ), mpg123_length( p_handle ) );

					return true;
				}
			}
		}

		mpg123_close( p_handle );
		mpg123_delete( p_handle );

		return false;
	}

	bool SoundPlayer::readMp3( char* p_data, const ALsizei p_count, mpg123_handle* p_handle, ALuint p_buffer ) {
		auto buffer = reinterpret_cast<unsigned char*>( p_data );
		size_t decodedBytes = 0;

		int ret = mpg123_decode( p_handle, NULL, 0, buffer, p_count, &decodedBytes );
		if ( ret > MPG123_OK && ret != MPG123_NEED_MORE ) {
			wxLogMessage( wxT( "Decoding error %d" ), ret );
			return false;
		}

		if ( decodedBytes == 0 ) {
			return false;
		}

		// Buffer decoded sound data to OpenAL buffer
		alBufferData( p_buffer, m_format, p_data, decodedBytes, m_frequency );
		//wxLogMessage( wxT( "Read %d bytes" ), decodedBytes );
		return true;
	}

	bool SoundPlayer::playSoundThread( const int p_index ) {
		// Copy the data to this thread function, since some times the thread isn't finish
		// but the data already destroyed when play new file
		auto localData = m_sound[p_index].data;

		auto data = localData.GetPointer( );
		auto size = localData.GetSize( );

		if ( p_index < m_listCtrl->GetItemCount( ) ) {
			// Set first column text to indicate playing
			m_listCtrl->SetItem( p_index, 0, wxT( ">" ) );
		}

		m_isPlaying = true;
		m_isThreadEnded = false;

		// Set sound orientation
		{
			// listener position
			alListener3f( AL_POSITION, m_listenerPos.x, m_listenerPos.y, m_listenerPos.z );
			// listener velocity
			alListener3f( AL_VELOCITY, m_listenerVel.x, m_listenerVel.y, m_listenerVel.z );
			// listener orientation
			alListenerfv( AL_ORIENTATION, m_listenerOri );
		}

		// Generate source
		alGenSources( 1, &m_source );

		// Set sound source
		{
			// source pitch
			alSourcef( m_source, AL_PITCH, m_sourcePitch );
			// source gain
			alSourcef( m_source, AL_GAIN, m_sourceGain );
			// source position
			alSource3f( m_source, AL_POSITION, m_sourcePos.x, m_sourcePos.y, m_sourcePos.z );
			// source velocity
			alSource3f( m_source, AL_VELOCITY, m_sourceVel.x, m_sourceVel.y, m_sourceVel.z );
			// source looping
			alSourcei( m_source, AL_LOOPING, AL_FALSE );
		}

		// Oggs data
		OggVorbis_File oggFile;
		ogg_file oggStream;
		ov_callbacks oggCallbacks;

		// mpg123 handle
		mpg123_handle* mpg123Handle = nullptr;

		auto const fourcc = *reinterpret_cast<uint32*>( data );
		if ( fourcc == FCC_OggS ) {
			if ( !this->loadOggs( reinterpret_cast<char*>( data ), size, &oggFile, oggStream, oggCallbacks ) ) {
				wxLogMessage( wxT( "Error loading Oggs data." ) );
				return false;
			}
		} else if ( ( fourcc & 0xffff ) == FCC_MP3 ) {
			int error;
			// Initialize mpg123 library
			mpg123_init( );
			// Create new mpg123 handle
			mpg123Handle = mpg123_new( NULL, &error );
			if ( mpg123Handle ) {
				if ( !this->loadMp3( reinterpret_cast<char*>( data ), size, mpg123Handle ) ) {
					wxLogMessage( wxT( "Error loading MP3 data." ) );
					return false;
				}
			}
		}

		// Allocate buffer
		auto buf = allocate<char>( m_bufferSize * sizeof( ALshort ) );

		bool ret = false;

		// Buffer audio data to OpenAL
		for ( int i = 0; i < NUM_BUFFERS; i++ ) {
			if ( fourcc == FCC_OggS ) {
				ret = this->readOggs( buf, m_bufferSize, &oggFile, m_buffers[i] );
			} else if ( ( fourcc & 0xffff ) == FCC_MP3 ) {
				ret = this->readMp3( buf, m_bufferSize, mpg123Handle, m_buffers[i] );
			}

			if ( alGetError( ) != AL_NO_ERROR ) {
				freePointer( buf );
				wxLogMessage( wxT( "Error buffering sound data." ) );
				return false;
			}
		}

		// Queue the buffers onto the source
		alSourceQueueBuffers( m_source, NUM_BUFFERS, m_buffers );

		// Start playback
		alSourcePlay( m_source );
		if ( alGetError( ) != AL_NO_ERROR ) {
			freePointer( buf );
			wxLogMessage( wxT( "Error start playing sound." ) );
			return false;
		}

		ALint state;
		alGetSourcei( m_source, AL_SOURCE_STATE, &state );

		while ( ret && m_isPlaying ) {
			ALuint buffer;
			ALint bufferProcessed;
			ALint queuedBuffers;

			// sleep 10ms to make it not use too much CPU time
			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );

			// Check if OpenAL is processed the buffer
			alGetSourcei( m_source, AL_BUFFERS_PROCESSED, &bufferProcessed );

			if ( bufferProcessed ) {
				// For each processed buffer
				while ( bufferProcessed-- ) {
					// Remove processed buffer from the source queue
					alSourceUnqueueBuffers( m_source, 1, &buffer );
					// Read the next chunk of decoded audio data and fill the old buffer with new data
					if ( fourcc == FCC_OggS ) {
						ret = this->readOggs( buf, m_bufferSize, &oggFile, buffer );
					} else if ( ( fourcc & 0xffff ) == FCC_MP3 ) {
						ret = this->readMp3( buf, m_bufferSize, mpg123Handle, buffer );
					}

					if ( ret ) {
						// Insert the buffer to the source queue
						alSourceQueueBuffers( m_source, 1, &buffer );
						if ( alGetError( ) != AL_NO_ERROR ) {
							wxLogMessage( wxT( "Error buffering sound data." ) );
							break;
						}
					}
				}
			}

			alGetSourcei( m_source, AL_SOURCE_STATE, &state );
			if ( state != AL_PLAYING ) {
				alGetSourcei( m_source, AL_BUFFERS_QUEUED, &queuedBuffers );
				if ( queuedBuffers ) {
					alSourcePlay( m_source );
				} else {
					break;
				}
			}
		}

		if ( fourcc == FCC_OggS ) {
			ov_clear( &oggFile );
		} else if ( ( fourcc & 0xffff ) == FCC_MP3 ) {
			if ( mpg123_close( mpg123Handle ) != MPG123_OK ) {
				wxLogMessage( wxT( "Error closing mpg123 handle." ) );
			}
			mpg123_delete( mpg123Handle );
			mpg123_exit( );
		}

		// Delete source
		alDeleteSources( 1, &m_source );

		// Clean up
		freePointer( buf );

		m_isPlaying = false;
		m_isThreadEnded = true;

		if ( p_index < m_listCtrl->GetItemCount( ) ) {
			// Reset first column text
			m_listCtrl->SetItem( p_index, 0, wxT( "*" ) );
		}

		return true;
	}

	void SoundPlayer::playSound( const int p_index ) {
		// Stop playing sound, if still playing it
		this->stopSound( );
		// Play the first entry
		m_thread = std::thread( &SoundPlayer::playSoundThread, this, p_index );
		m_thread.detach( );
	}

	void SoundPlayer::stopSound( ) {
		// Is the sound still playing?
		if ( this->playing( ) ) {
			// Stop OpenAL to play sound
			alSourceStop( m_source );
			// Signal player thread to stop playing
			m_isPlaying = false;
		}
		// Wait until the thread is finished
		while ( !m_isThreadEnded ) {
			std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
		}
	}

	bool SoundPlayer::playing( ) {
		return m_isPlaying;
	}

	void SoundPlayer::setVolume( const ALfloat p_vol ) {
		alListenerf( AL_GAIN, p_vol );
	}

	void SoundPlayer::onListItemDoubleClickedEvt( wxListEvent& p_event ) {
		auto index = p_event.m_itemIndex;
		this->playSound( index );
	}

	void SoundPlayer::onButtonEvt( wxCommandEvent& p_event ) {
		auto id = p_event.GetId( );
		switch ( id ) {
		case ID_BtnBack:
			this->onPrev( );
			break;
		case ID_BtnPlay:
			this->onPlay( );
			// Todo:change play button label
			break;
		case ID_BtnStop:
			this->onStop( );
			break;
		case ID_BtnForward:
			this->onNext( );
			break;
		}
	}

	void SoundPlayer::onPlay( ) {
		long index = -1;
		if ( m_listCtrl->GetItemCount( ) == 0 ) {
			return;
		}
		while ( ( index = m_listCtrl->GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED ) ) != wxNOT_FOUND ) {
			this->playSound( index );
		}
	}

	void SoundPlayer::onPause( ) {
		// Pause the sound
		alSourcePause( m_source );
	}

	void SoundPlayer::onStop( ) {
		this->stopSound( );
	}

	void SoundPlayer::onNext( ) {
		long index = -1;

		auto count = m_listCtrl->GetItemCount( );
		if ( count == 0 ) {
			return;
		}

		while ( ( index = m_listCtrl->GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED ) ) != wxNOT_FOUND ) {
			index += 1;
			if ( index > ( count - 1 ) ) {
				index = 0;
			}
			this->selectEntry( index );
			this->playSound( index );
		}
	}

	void SoundPlayer::onPrev( ) {
		long index = -1;

		auto count = m_listCtrl->GetItemCount( );
		if ( count == 0 ) {
			return;
		}

		while ( ( index = m_listCtrl->GetNextItem( index, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED ) ) != wxNOT_FOUND ) {
			index -= 1;
			if ( index < 0 ) {
				index = count - 1;
			}
			this->selectEntry( index );
			this->playSound( index );
		}
	}

	void SoundPlayer::onVolChange( wxCommandEvent& WXUNUSED( p_event ) ) {
		this->setVolume( m_volSlider->GetValue( ) / 100.0 );
	}

}; // namespace gw2b
