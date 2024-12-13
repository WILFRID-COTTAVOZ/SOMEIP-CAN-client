#include <cstring>  // for memcpy   
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <vsomeip/vsomeip.hpp>
#include "sample-ids.hpp"

class client_sample {
public:
    client_sample(bool _use_tcp, bool _be_quiet, uint32_t _cycle)
        : app_(vsomeip::runtime::get()->create_application()),
          request_(vsomeip::runtime::get()->create_request(_use_tcp)),
          use_tcp_(_use_tcp),
          be_quiet_(_be_quiet),
          cycle_(_cycle),
          running_(true),
          blocked_(false),
          is_available_(false),
          sender_(std::bind(&client_sample::run, this)) {
    }

    bool init() {
        if (!app_->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        std::cout << "Client settings [protocol="
                  << (use_tcp_ ? "TCP" : "UDP")
                  << ":quiet="
                  << (be_quiet_ ? "true" : "false")
                  << ":cycle="
                  << cycle_
                  << "]"
                  << std::endl;

        app_->register_state_handler(
                std::bind(
                    &client_sample::on_state,
                    this,
                    std::placeholders::_1));

        app_->register_message_handler(
                vsomeip::ANY_SERVICE, SAMPLE_INSTANCE_ID, vsomeip::ANY_METHOD,
                std::bind(&client_sample::on_message,
                          this,
                          std::placeholders::_1));

        request_->set_service(SAMPLE_SERVICE_ID);
        request_->set_instance(SAMPLE_INSTANCE_ID);
        request_->set_method(SAMPLE_METHOD_ID);

        app_->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
                std::bind(&client_sample::on_availability,
                          this,
                          std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        return true;
    }

    void start() {
        app_->start();
    }

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    void stop() {
        running_ = false;
        blocked_ = true;
        app_->clear_all_handler();
        app_->release_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        condition_.notify_one();
        if (std::this_thread::get_id() != sender_.get_id()) {
            if (sender_.joinable()) {
                sender_.join();
            }
        } else {
            sender_.detach();
        }
        app_->stop();
    }
#endif

    void on_state(vsomeip::state_type_e _state) {
        if (_state == vsomeip::state_type_e::ST_REGISTERED) {
            app_->request_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID);
        }
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) 
    {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                << "] is "
                << (_is_available ? "available." : "NOT available.")
                << std::endl;

        if (SAMPLE_SERVICE_ID == _service && SAMPLE_INSTANCE_ID == _instance) 
	  {
            if (is_available_  && !_is_available) 
	      {
                is_available_ = false;
              }
	    else if (_is_available && !is_available_) 
	      {
                is_available_ = true;
                send_request();
              }
          }
    }



    //RECEIPT OF RESPONSE MESSAGE
    //***************************
   
    void on_message(const std::shared_ptr<vsomeip::message> &_response) 
    {
      std::shared_ptr<vsomeip::payload> its_payload = _response->get_payload();
      std::vector<vsomeip::byte_t> its_payload_data(its_payload->get_length());
      
      std::string result_text;

      // Copy data using memcpy
      std::memcpy(its_payload_data.data(), its_payload->get_data(), its_payload->get_length());

      if (its_payload_data.size() == 3) 
        {
          // Interpret the 3-byte result
          uint8_t result = (static_cast<uint8_t>(its_payload_data[0]));

	  //WC extract Response sequence number (shall match the Request sequence number)
          uint16_t Rsp_seqnum = (static_cast<uint16_t>(its_payload_data[1]) << 8) |
                                (static_cast<uint16_t>(its_payload_data[2]));

          // Display the result based on the last operation
	
	  if (result==2)
	    {
               result_text = " was done successfully !";
	    }
	  else
            {
               result_text = " has failed !";
	    }
	  std::cout << "Request [" << Rsp_seqnum << "]" << result_text << std::endl;
	
	} 
      else 
        {
           std::cerr << "Error: Response payload size is incorrect. Expected 3 bytes, got " << its_payload_data.size() << " bytes." << std::endl;
        }
        std::cout << std::endl << "*************************************" << std::endl;
    
    
      if (is_available_)
          send_request();
    }



       //BUILD REQUEST
       //*************
       
    void send_request() {
        std::string input_line;
        std::string input_LED_pos;
        std::string input_op_param;
        char operation;
	bool op_valid;
	bool LED_pos_valid;
	bool op_param_valid;
	int LED_position;
        long op_parameter;

	op_valid=false;
	while (op_valid==false)
        {
            // Prompt user to enter the operation
            std::cout << "1 = switch LED on, 2 = switch LED off, 3 = custom flash LED, 4 = standard flash LED" << std::endl;
            std::cout << "Please enter your operation or q to quit : ";
            std::getline(std::cin, input_line);

	    //In case the user wants to quit the app
	    std::string first_char_op=input_line.substr(0,1);
	    if (first_char_op.compare("q")==0)
            { 
              exit(0);
	    }

            // Check if the input line contains a valid choice (1-4)
	    if (strchr("1234",first_char_op[0])!=NULL) 
	    {
	        op_valid=true;
                LED_pos_valid=false;
		while (LED_pos_valid==false)
	          {
		     std::cout << "which LED (1-8) [1] (q to quit) ? ";
		     std::getline(std::cin, input_LED_pos);
	             std::string first_char_LED_pos=input_LED_pos.substr(0,1);
	             
		     //Quit ?
		     if (first_char_LED_pos.compare("q")==0)
                       { 
                          exit(0);
	               }
		     
		     //Default value
		     if (input_LED_pos.size()==0) 
		       {
                          LED_position=1;
	                  LED_pos_valid=true;
		       }
		     else

		     //Valid LED position (1-8) ?     
	               {   
	                  if (strchr("12345678",input_LED_pos[0])!=NULL) 
	                    {
		               int CHR=48;
                               LED_position=input_LED_pos[0]-CHR;
	                       LED_pos_valid=true;
	                    }
	               }
	          }

		op_parameter=250;    //Default value
                op_param_valid=false;
		if (first_char_op.compare("3")==0)
                  {
		     while (op_param_valid==false)
	               {
		          std::cout << "which period (100-5000 ms) [250] (q to quit) ? ";
		          std::getline(std::cin, input_op_param);
	                  std::string first_char_op_param=input_op_param.substr(0,1);
	             
		          //Quit ?
		          if (first_char_op_param.compare("q")==0)
                            { 
                               exit(0);
	                    }
		     
		          //Default value
		          if (input_op_param.size()==0) 
		            {
                               op_parameter=250;
	                       op_param_valid=true;
		            }
		         else

		         //Valid period (100-5000) ?     
	                   {   
	                      if ((stoul(input_op_param) >= 100) && (stoul(input_op_param) <= 5000)) 
	                        {
                                   op_parameter=stoul(input_op_param);
	                           op_param_valid=true;
	                        }
	                   }
		       }
		 }
            }		
            
	    //In case the choice is not valid
            if (op_valid == false) 
	    {
              std::cerr << "Error: Invalid input.";
              std::cout << std::endl << "*************************************" << std::endl;

              continue;
            }
            
            operation = first_char_op[0];
        } 

        // Prepare the payload: 6 bytes
	// 1 byte for the operation (1 2 3 or 4)
	// 1 byte for the LED position (1 as default)
	// 2 bytes for the operation parameter (if operation = 3, 250 ms as default)
	// 2 bytes for the sequence number
        std::vector<vsomeip::byte_t> payload_data(6);

        // Set the operation (1 byte)
        payload_data[0] = static_cast<vsomeip::byte_t>(operation);

        // Set the LED position (1 byte)
        payload_data[1] = static_cast<vsomeip::byte_t>(LED_position);

        // Set the operation parameter (2 bytes)
        payload_data[2] = static_cast<vsomeip::byte_t>((op_parameter >> 8) & 0xFF);
        payload_data[3] = static_cast<vsomeip::byte_t>(op_parameter & 0xFF);

        // Set the second operand (2 bytes)
        payload_data[4] = static_cast<vsomeip::byte_t>((Req_seqnum_ >> 8) & 0xFF);
        payload_data[5] = static_cast<vsomeip::byte_t>(Req_seqnum_ & 0xFF);

        // Create a payload from the binary data
        std::shared_ptr<vsomeip::payload> its_payload = vsomeip::runtime::get()->create_payload();
        its_payload->set_data(payload_data);

        // Set the payload on the request message
        request_->set_payload(its_payload);

	
	switch (operation) {
            case '1':
                std::cout << "Sending request [" << Req_seqnum_ << "] : Switch on LED " << std::to_string(LED_position) << std::endl;
                break;
            case '2':
                std::cout << "Sending request [" << Req_seqnum_ << "] : Switch off LED " << std::to_string(LED_position) << std::endl;
                break;
            case '3':
                std::cout << "Sending request [" << Req_seqnum_ << "] : Flash LED " << std::to_string(LED_position) << " ("<< std::to_string(op_parameter) << " ms)" << std::endl;
                break;
            case '4':
                std::cout << "Sending request [" << Req_seqnum_ << "] : Flash LED " << std::to_string(LED_position) << " (standard)" << std::endl;
                break;
            default:
                std::cerr << "Error: Unknown operation." << std::endl;
                break;
        }

        if (!be_quiet_) {
            std::lock_guard<std::mutex> its_lock(mutex_);
            blocked_ = true;
            condition_.notify_one();
        }
    }



    void run() {
	Req_seqnum_= 0;                            //WC init Req_seqnum_
        while (running_) {
            std::unique_lock<std::mutex> its_lock(mutex_);
            while (!blocked_) condition_.wait(its_lock);
            if (is_available_) {
                app_->send(request_);
		Req_seqnum_++;               //WC increment Req_seqnum_
                blocked_ = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(cycle_));
        }
    }

private:
    std::shared_ptr<vsomeip::application> app_;
    std::shared_ptr<vsomeip::message> request_;
    bool use_tcp_;
    bool be_quiet_;
    uint32_t cycle_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool running_;
    bool blocked_;
    bool is_available_;
    std::thread sender_;
    
    uint16_t Req_seqnum_; // To store the request sequence number
    uint16_t Packet_loss_; // To store the total packet loss counter
};

#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    client_sample *its_sample_ptr(nullptr);
    void handle_signal(int _signal) 
    {
        if (its_sample_ptr != nullptr &&
                (_signal == SIGINT || _signal == SIGTERM))
            its_sample_ptr->stop();
    }
#endif

int main(int argc, char **argv) {
    bool use_tcp = false;
    bool be_quiet = false;
    uint32_t cycle = 1000;

    std::string tcp_enable("--tcp");
    std::string quiet_enable("--quiet");
    std::string cycle_arg("--cycle");

    int i = 1;
    while (i < argc) {
        if (tcp_enable == argv[i]) {
            use_tcp = true;
        } else if (quiet_enable == argv[i]) {
            be_quiet = true;
        } else if (cycle_arg == argv[i] && i+1 < argc) {
            i++;
            std::stringstream converter;
            converter << argv[i];
            converter >> cycle;
        }
        i++;
    }

    client_sample its_sample(use_tcp, be_quiet, cycle);
#ifndef VSOMEIP_ENABLE_SIGNAL_HANDLING
    its_sample_ptr = &its_sample;
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
#endif
    if (its_sample.init()) {
        its_sample.start();
        return 0;
    } else {
        return 1;
    }
}
