/*
 * Queue handler.
 *
 */
#define IO_QUEUE_LENGTH 64000

static unsigned char input_queue[IO_QUEUE_LENGTH];
static unsigned char output_queue[IO_QUEUE_LENGTH];
static int in_head_pointer;
static int in_tail_pointer;
static int out_head_pointer;
static int out_tail_pointer;
static int in_full_flag;
static int in_empty_flag;
static int out_full_flag;
static int out_empty_flag;

void reset_input_queue(void)
{
	in_head_pointer=in_tail_pointer = 0;
	in_full_flag  = 0;
	in_empty_flag = 1;
}
void reset_output_queue(void)
{
	out_head_pointer=out_tail_pointer = 0;
	out_full_flag  = 0;
	out_empty_flag = 1;
}
int get_input_queue_length(void)
{
	int length;
	
	if( in_full_flag == 1 )
	{
		length = IO_QUEUE_LENGTH;
	}
	else
	{
		if( in_head_pointer >= in_tail_pointer )
		{
			length = in_head_pointer - in_tail_pointer;
		}
		else
		{
			length = in_head_pointer - in_tail_pointer + IO_QUEUE_LENGTH;
		} 
	}
	return length;
}
int get_output_queue_length(void)
{
	int length;
	
	if( out_full_flag == 1)
	{
		length = IO_QUEUE_LENGTH;
	}
	else
	{
		if( out_head_pointer >= out_tail_pointer )
		{
			length = out_head_pointer - out_tail_pointer;
		}
		else
		{
			length = out_head_pointer - out_tail_pointer + IO_QUEUE_LENGTH;
		} 
	}
	return length;
}
int write_input_queue( unsigned char *data, int length )
{
	int i = 0;

	if( ( length != 0 )  && ( in_full_flag == 0 ) )
	{
		in_empty_flag = 0;
		for( i = 0; i < length; i++ )
		{
			input_queue[in_head_pointer] = data[i];
			in_head_pointer = ++in_head_pointer%IO_QUEUE_LENGTH;
			if( in_head_pointer == in_tail_pointer )
			{
				in_full_flag = 1;
				i++;
				break;
			}
		}
	}
	return i;
}
int write_output_queue( unsigned char *data, int length )
{
	int i = 0;

	if( ( length != 0 ) && ( out_full_flag == 0 ) )
	{
		out_empty_flag = 0;
		for( i = 0; i < length; i++ )
		{
			output_queue[out_head_pointer] = data[i];
			out_head_pointer = ++out_head_pointer%IO_QUEUE_LENGTH;
			if( out_head_pointer == out_tail_pointer )
			{
				out_full_flag = 1;
				i++;
				break;
			}
		}
	}
	return i;
}
int read_input_queue( unsigned char *data, int length )
{
	int i = 0;

	if( (length != 0) && (in_empty_flag == 0) )
	{
		in_full_flag = 0;
		for( i = 0; i < length; i++ )
		{
			data[i] = input_queue[in_tail_pointer];
			in_tail_pointer = ++in_tail_pointer%IO_QUEUE_LENGTH;
			if( in_tail_pointer == in_head_pointer )
			{
				in_empty_flag = 1;
				i++;
				break;
			}
		}
	}
	return i;	
}
int read_output_queue( unsigned char *data, int length )
{
	int i = 0;

	if( (length != 0) && ( out_empty_flag == 0) )
	{
		out_full_flag = 0;
		for( i = 0; i < length; i++ )
		{
			data[i] = output_queue[out_tail_pointer];
			out_tail_pointer = ++out_tail_pointer%IO_QUEUE_LENGTH;
			if( out_tail_pointer == out_head_pointer )
			{
				out_empty_flag = 1;
				i++;
				break;
			}
		}
	}
	return i;	
}


