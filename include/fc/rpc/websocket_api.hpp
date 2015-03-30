#pragma once
#include <fc/rpc/api_connection.hpp>
#include <fc/rpc/state.hpp>
#include <fc/network/http/websocket.hpp>
#include <fc/io/json.hpp>
#include <fc/reflect/variant.hpp>

namespace fc { namespace rpc {

   class websocket_api_connection : public api_connection
   {
      public:
         websocket_api_connection( fc::http::websocket_connection& c )
         :_connection(c)
         {
            _rpc_state.add_method( "call", [this]( const variants& args ) -> variant {
                      FC_ASSERT( args.size() == 3 && args[2].is_array() );
                      return this->receive_call( args[0].as_uint64(),
                                                         args[1].as_string(),
                                                         args[2].get_array() );
                                  });
            _connection.on_message_handler( [&]( const std::string& msg ){ on_message(msg); } );
         }

         virtual variant send_call( api_id_type api_id, 
                                    const string& method_name, 
                                    const variants& args = variants() ) override
         {
            auto request = _rpc_state.start_remote_call(  "call", {api_id, method_name, args} );
            _connection.send_message( fc::json::to_string(request) );
            return _rpc_state.wait_for_response( *request.id );
         }

      protected:
         void on_message( const std::string& message )
         {
            auto var = fc::json::from_string(message);
            const auto& var_obj = var.get_object();
            if( var_obj.contains( "method" ) )
            {
               auto call = var.as<fc::rpc::request>();
               try {
                  auto result = _rpc_state.local_call( call.method, call.params );
                  if( call.id )
                  {
                     _connection.send_message( fc::json::to_string( response( *call.id, result ) ) );
                  }
               }
               catch ( const fc::exception& e )
               {
                  if( call.id )
                  {
                     _connection.send_message( fc::json::to_string( response( *call.id,  error_object{ 1, e.to_detail_string(), fc::variant(e)}  ) ) );
                  }
               }
            }
            else 
            {
               auto reply = var.as<fc::rpc::response>();
               _rpc_state.handle_reply( reply );
            }
         }
         fc::http::websocket_connection&  _connection;
         fc::rpc::state                   _rpc_state;
   };

} } // namespace fc::rpc
