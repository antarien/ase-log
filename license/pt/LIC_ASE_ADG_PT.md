# Contrato de Licença da Antares Simulation Engine e do ADG

**Tradução para o português do Brasil da versão em inglês; identificadores e conteúdo normativo inalterados.**

**Identificador: ASE-ADG-LIC-2026-10-16-1.0**  
**Data de emissão: 16 de outubro de 2026**  
**Licenciadora contratante: ANTARES SIMULACAO LTDA — CNPJ 69.363.366/0001-21**

Esta é uma licença própria. Não é a Licença MIT, a Licença Apache nem uma licença de código aberto irrestrita. O acesso público ao código-fonte não afasta as condições de contribuição e as condições comerciais abaixo. Nenhuma assinatura do backend é exigida simplesmente para baixar, bifurcar (fork), utilizar ou desenvolver Componentes Públicos nos termos deste Contrato.

## 1. Partes, poderes e escopo

1.1 “Licenciadora” significa ANTARES SIMULACAO LTDA, com sede em Rua Milton de Brito Firmeza 180, Icaraí, Caucaia, Ceará, CEP 61624-365, Brasil. “Você” significa a pessoa física ou jurídica que aceita este Contrato. Quem age por uma pessoa jurídica deve identificá-la e ter poderes para vinculá-la. Entidades relacionadas não são automaticamente o mesmo licenciado.

1.2 A Licenciadora contrata em seu próprio nome. Jan Ohlmann conserva os direitos que detém em caráter privado e é o fundador e autor do conceito. Sua menção em avisos de direitos autorais ou como instância de aprovação do projeto não o torna pessoalmente responsável pelos serviços da Licenciadora aos clientes. Os poderes da Licenciadora para licenciar os materiais privados dele decorrem de um contrato separado; este documento, por si só, não transfere esses materiais à sociedade.

1.3 Este Contrato aplica-se aos materiais expressamente disponibilizados sob o seu identificador. Um arquivo de licença em um repositório superior não substitui uma licença independente em um sub-repositório ou em uma dependência de terceiros. A Licenciadora concede somente os direitos que validamente detém. Direitos legais e direitos obtidos de forma independente sob outras licenças não são afastados.

1.4 “ADG” significa a comunidade de projeto Antares Developer Group, e não uma pessoa jurídica distinta apenas em razão do seu nome. Membros do ADG são as pessoas físicas e jurídicas admitidas nos termos do Acordo de Participação no Projeto, `PJV.md`, identificador ASE-ADG-PJV-2026-10-16-1.0. O mero uso público não exige nem cria a condição de Membro.

1.5 O projeto diz respeito à Antares Simulation Engine (“Engine”) e ao seu ecossistema. Antares Open World é um projeto dentro desse ecossistema. O uso autorizado não se limita a esse jogo específico.

## 2. Componentes, extensões e seus próprios produtos

2.1 “Componentes” incluem o código da Engine e os módulos, plugins, definições de interface, materiais de build e documentação associados, expressamente abrangidos por este Contrato. Materiais licenciados de forma independente mantêm suas licenças e avisos.

2.2 Uma “Extensão do Ecossistema” é uma modificação de um Componente, ou um novo módulo ou plugin que amplia, adapta ou fornece funcionalidade da Engine, inclusive por meio da API da Engine para um cliente. Isso inclui um módulo escrito exclusivamente para o seu próprio produto e uma nova implementação que não contenha código-fonte copiado da Engine. A obrigação de contribuir com essa extensão é uma condição contratual; ela não depende de a extensão ser uma obra derivada para fins de direito autoral.

2.3 O seu “Produto” é o seu próprio jogo, aplicativo cliente ou ferramenta. O código-fonte independente, a interface, o conteúdo de jogo, os assets e a lógica específica do produto continuam pertencendo a você ou aos respectivos titulares. A simples chamada de uma API não transforma um Produto inteiro em uma contribuição. Inversamente, colocar um módulo da Engine dentro de um repositório ou executável de cliente não isenta esse módulo. O que determina o limite é a finalidade funcional, e não o nome ou o empacotamento. Uma biblioteca independente preexistente não é automaticamente contribuída apenas por ser utilizada; quaisquer permissões necessárias devem, ainda assim, estar disponíveis.

2.4 Por exemplo, um editor de paisagens continua sendo o Produto de quem o desenvolveu. Um módulo de erosão desenvolvido para ele e conectado pela API da Engine é uma Extensão do Ecossistema e deve ser contribuído. O código-fonte independente do editor não precisa ser divulgado.

2.5 Uma “Contribuição” é um estado de desenvolvimento efetivo apresentado por meio do processo de contribuição do projeto, incluindo versões iniciais, incompletas e intermediárias. O termo não inclui automaticamente toda obra que um contribuidor possa criar no futuro.

## 3. Aceitação e informações de publicação

3.1 Este Contrato é oferecido com os Componentes pertinentes. Você deve ter a oportunidade de lê-lo antes de aceitá-lo. Você o aceita por meio de confirmação eletrônica expressa ou, quando juridicamente eficaz, exercendo conscientemente as permissões aqui oferecidas após receber os termos. Um simples download não é tratado como prova conclusiva de que cada encargo contratual foi conscientemente aceito. Direitos de acesso ou de plataforma concedidos de forma independente pelo GitHub ou pela legislação imperativa não são afetados.

3.2 Antes do acesso privado ou da sua primeira contribuição, o processo do projeto registra a sua identidade, a eventual entidade representada, as versões da LIC e do PJV e o repositório ou a tarefa aprovados. Um commit e push iniciais autorizados, realizados após a apresentação das condições, podem registrar a aceitação sem um contrato em papel para cada commit. O nome do autor de um commit, por si só, não prova poderes para licenciar o código de um empregador.

3.3 As condições comerciais das seções 9–11 devem ser apresentadas e aceitas antes da monetização. Basta uma confirmação que mencione esta versão da licença; não é necessária a negociação de uma taxa adicional de participação. O download público não o torna assinante do PaaS.

3.4 O aviso aplicável do repositório ou do componente identifica a publicação pública ou restrita, eventuais avisos separados e, para acesso restrito, a autorização que define os destinatários e a duração. Uma publicação pública autorizada nos termos deste Contrato é um “Componente Público”. Um vazamento, uma configuração de acesso equivocada ou um upload não autorizado não é uma publicação pública. Uma alteração posterior da visibilidade do repositório não revoga direitos já concedidos sobre Componentes Públicos recebidos licitamente.

## 4. Permissões públicas

4.1 Sujeita a este Contrato, a Licenciadora concede a você uma licença mundial e não exclusiva para baixar, clonar, bifurcar (fork), copiar, estudar, executar, modificar e desenvolver Componentes Públicos, e para utilizá-los em seus próprios Produtos. Essas permissões não estão condicionadas à compra ou manutenção de uma assinatura do backend. Elas permanecem disponíveis após cancelamento ou redução de plano, desde que este Contrato seja cumprido.

4.2 Você pode redistribuir Componentes Públicos, inclusive versões modificadas, mantendo intactos este Contrato, os avisos corretos de direitos autorais e de atribuição e os avisos de terceiros aplicáveis. Os destinatários devem receber os termos de licença aplicáveis. Você não pode apresentar Componentes privados como publicados publicamente nem impor termos que impeçam os destinatários de exercer as permissões concedidas pelo licenciante efetivo. O seu fork não substitui os deveres de contribuição ao projeto previstos na seção 6.

4.3 O desenvolvimento público pode ser não comercial ou comercial. O relatório do Produto e a participação sobre receita aplicam-se conforme especificado abaixo. Não é necessária permissão individual para cada uso conforme de Componentes Públicos, exceto a aprovação prévia do projeto exigida para novos módulos e plugins contribuídos. A reutilização pública por uma pessoa que não seja Membro do ADG não concede acesso a repositórios privados do ADG.

4.4 Sujeitas ao cumprimento, as permissões públicas são concedidas pelo prazo dos direitos de propriedade intelectual pertinentes. A interrupção da distribuição ou da condição de Membro, por si só, não as cancela. Este Contrato não é uma promessa de versões futuras, hospedagem gratuita, suporte ou aceitação de uma contribuição.

## 5. Permissões restritas e Enterprise

5.1 Um Componente restrito só pode ser utilizado dentro da sua autorização documentada. Uma autorização comunitária pode decorrer de uma função aprovada no projeto; uma autorização Enterprise decorre do respectivo contrato de cliente. Nenhuma delas concede acesso a todos os repositórios privados.

5.2 O cliente autorizado pode permitir que seu pessoal e seus contratados devidamente autorizados trabalhem para ele dentro dessa autorização, sujeitos à confidencialidade e ao cumprimento da licença. Isso não é uma licença independente para essas pessoas, para afiliadas, publicadoras ou para os clientes do cliente. Uma função comunitária pessoal identificada separadamente não deve ser usada para contornar restrições de um empregador ou de um cliente.

5.3 Uma autorização Enterprise permite a inspeção, o desenvolvimento e a adaptação especificados de código privado durante o seu prazo autorizado. A posse de uma cópia baixada não prorroga esse prazo. Redistribuir código-fonte privado, expô-lo por meio de um fork público, conceder a outra empresa direitos independentes de desenvolvimento da Engine ou vender uma sublicença da Engine não é autorizado por uma assinatura Enterprise comum.

5.4 Este Contrato não concede o direito de operar um servidor dedicado da Engine ou um serviço PaaS independente da Engine. Tais direitos de operação exigem uma concessão expressa e separada. A execução autorizada do lado do cliente e a paridade de simulação offline não constituem, apenas por esse motivo, operação de servidor dedicado. Direitos independentes sob uma licença diferente não são afetados.

## 6. Aprovação e contribuição contínua

6.1 Antes de iniciar uma nova Extensão do Ecossistema, submeta sua finalidade, seu escopo e o limite proposto do módulo ou plugin à aprovação de Jan Ohlmann ou de uma instância do projeto expressamente autorizada por ele. O trabalho em componentes existentes deve seguir a tarefa registrada e o processo de revisão. A aprovação considera a finalidade de simulação da Engine, os padrões publicados do projeto e as restrições éticas comunicadas. Não existe direito de forçar a inclusão de conteúdo arbitrário no projeto.

6.2 Uma vez aprovada, a extensão deve ser inicializada no repositório do projeto que lhe for atribuído e vinculada à estrutura prescrita de superrepositório/sub-repositório. As credenciais autorizadas são fornecidas pelo processo de acesso. O desenvolvimento deve ser visível ali desde o seu estado inicial, e não apenas quando concluído ou lançado comercialmente.

6.3 Você deve fazer commit e push dos estados de desenvolvimento efetivos de forma contínua, como parte do fluxo de desenvolvimento. A edição local continua possível, mas reter o processo de desenvolvimento em um fork interno até uma entrega final não constitui cumprimento. Forks e espelhos de empresas são permitidos dentro de suas permissões de acesso, mas não substituem o repositório canônico de contribuição. Esta cláusula não impõe prazo diário ou horário não anunciado.

6.4 As Contribuições devem seguir os padrões documentados aplicáveis de arquitetura, codificação, interface, versionamento e documentação, e incluir o código-fonte editável, o README, as informações necessárias de build e integração e os avisos de licença. O padrão aplicável é aquele comunicado para o trabalho aprovado; um padrão posterior não reescreve tacitamente obrigações passadas.

6.5 Contribuições de empresas, empregados, pessoas físicas e da comunidade seguem as mesmas regras. Uso interno, ausência de vendas e integração apenas ao seu próprio Produto não isentam uma Extensão do Ecossistema. Os Produtos em si continuam excluídos nos termos da seção 2.

6.6 Disponibilize as contribuições aos destinatários autorizados para aquele repositório do projeto. Isso não exige publicar na internet código restrito da Engine. Jan, a Licenciadora e os Membros do ADG autorizados para o Componente pertinente devem poder obter e utilizar o desenvolvimento contribuído nos termos da seção 7.

6.7 Não faça commit de senhas, chaves de produção, dados pessoais de clientes ou segredos de terceiros. Forneça, em vez disso, informações seguras de configuração. Trabalhos sensíveis à segurança devem usar o canal protegido e autorizado do projeto. Uma indisponibilidade da plataforma deve ser comunicada e o trabalho acumulado sincronizado quando o acesso for restabelecido; a retenção deliberada não é uma indisponibilidade.

6.8 A integração ao branch principal é uma decisão de revisão separada. A concessão relativa à contribuição aplica-se às submissões autorizadas efetivas, incluindo estados intermediários, sem aguardar um merge. A aprovação ou o merge não podem legalizar uma submissão não autorizada de terceiros.

## 7. Direitos sobre contribuições e disponibilidade continuada

7.1 Para cada Contribuição que você esteja autorizado a submeter, você concede direitos não exclusivos e mundiais, pelo respectivo prazo de proteção aplicável, para copiá-la, executá-la, estudá-la, modificá-la, combiná-la, desenvolvê-la, reproduzi-la, distribuí-la dentro do escopo de publicação autorizado, explorá-la comercialmente e sublicenciá-la para o ecossistema da Engine e para Produtos conformes. Nenhuma remuneração separada pela contribuição é devida apenas porque esses direitos são exercidos. A remuneração do vínculo empregatício e remunerações acordadas separadamente não são afetadas.

7.2 A concessão beneficia expressamente Jan Ohlmann pessoalmente, a Licenciadora e os Membros do ADG autorizados para o Componente. Eles podem aceitar e fazer valer esse benefício na medida permitida pela legislação aplicável. O benefício de Jan independe da continuidade das operações da Licenciadora. A concessão desses direitos sobre contribuições não o torna prestador dos serviços do seu cliente.

7.3 Jan pode utilizar Contribuições em seu desenvolvimento privado da Engine e autorizar outros operadores da Engine; a Licenciadora pode utilizá-las em suas ofertas comerciais autorizadas de Engine, backend e Produtos; Membros do ADG autorizados podem utilizá-las dentro do ecossistema nos termos deste Contrato. Os limites de acesso privado continuam aplicáveis. As permissões de execução necessárias ao Produto podem ser repassadas aos usuários finais nos termos da seção 9. Operadores autorizados devem respeitar a disponibilidade das contribuições, a titularidade independente dos Produtos e os direitos existentes dos destinatários.

7.4 A concessão inclui a continuidade e o relicenciamento nos termos da seção 15. Você não pode revogar direitos concedidos sobre Contribuições apenas porque sai, fica inativo, muda de empregador, reduz o plano, cancela ou discorda do desenvolvimento subsequente. Você não precisa criar novas Contribuições após encerrar o seu envolvimento. Direitos legais imperativos não são afetados.

7.5 Você conserva a titularidade dos seus direitos detidos de forma independente. Este Contrato não afirma que Jan é o autor da sua obra, não lhe cede todas as obras futuras nem transfere a titularidade da Engine à Licenciadora. A atribuição correta e os direitos de autor protegidos permanecem intactos. Ele concede as permissões específicas necessárias ao uso compartilhado continuado.

7.6 Na medida em que você possa concedê-las, os direitos incluem as permissões necessárias de patente e de modelo de utilidade para usar, fabricar, mandar fabricar, modificar, distribuir e fornecer a Contribuição e as combinações autorizadas. Nenhuma nova cobrança de patente ou retirada decorre apenas da saída. Se você não tiver os poderes necessários, divulgue a limitação antes da submissão e não submeta material incompatível.

7.7 Os direitos sobre marcas, imagens e outros materiais incluídos estendem-se apenas ao seu uso autorizado com a Contribuição. Eles não permitem a apropriação geral da marca, da identidade ou do endosso de uma entidade. Mudanças de licença na saída para AGPL, Creative Commons ou outra licença não são desencadeadas por este Contrato.

## 8. Procedência, materiais de terceiros e responsabilidade

8.1 Você deve ser titular ou dispor de permissões suficientes para os direitos que concede, incluindo os direitos pertinentes decorrentes de relação de emprego, contratação, patentes e de terceiros. Isso se aplica também a trabalhos assistidos por IA. Divulgue dependências e condições externas antes da integração e preserve os avisos exigidos. Não submeta materiais cujas obrigações conflitem com o uso autorizado pretendido sem antes resolver esse conflito.

8.2 Você responde, nos termos da legislação aplicável, pelas violações que lhe sejam imputáveis. Na medida em que for lícito, você indeniza os beneficiários afetados por reclamações fundamentadas de terceiros e pelos custos razoáveis e necessários de defesa causados pela sua violação culposa dos requisitos de submissão e de permissão. A parte afetada deve notificá-lo prontamente, permitir uma participação razoável na defesa e evitar reconhecimentos ou acordos vinculantes sem consulta adequada. A conduta indevida dela própria não é transferida a você.

8.3 Nenhuma cláusula vincula um titular de direitos externo ou transfere responsabilidade penal. Uma submissão, um cabeçalho de direitos autorais ou a revisão da Licenciadora não sanam a falta de direitos. Material controvertido pode ser isolado durante a investigação sem extinguir os direitos sobre contribuições lícitas não relacionadas.

## 9. Produtos, publicação e distribuição a clientes

9.1 Você conserva o seu Produto independente. Essa isenção da contribuição de código-fonte não isenta o Produto do relatório nem da participação sobre receita. Para este Contrato, um Produto sujeito a essas condições é aquele que incorpora Componentes ou utiliza a funcionalidade da Engine deles em tempo de execução, localmente ou por meio do backend. O simples uso de um utilitário de desenvolvimento para criar um resultado de outra forma independente não torna, por si só, esse resultado um Produto sujeito à participação.

9.2 Você pode distribuir Produtos conformes e conceder aos usuários finais as permissões de instalação e de execução necessárias para utilizá-los. O runtime privado da Engine só pode ser incluído no executável ou em outra forma de execução expressamente designada como distribuível na publicação do Componente; o código-fonte privado de desenvolvimento e os direitos autônomos de desenvolvimento estão excluídos. O código-fonte público continua redistribuível nos termos da seção 4. Os usuários finais não adquirem acesso Enterprise ao comprar o seu Produto.

9.3 Publicadoras, lojas de aplicativos e distribuidores podem distribuir o Produto em seu nome dentro dos seus direitos. Uma condição de plataforma que exija direitos mais amplos sobre a Engine deve ser resolvida antes de você aceitar essa condição. Não se presume nenhuma relação de representação, endosso ou garantia pessoal de Jan.

9.4 Antes da primeira disponibilização externa do Produto, informe à Licenciadora a sua identidade, o Produto, os canais de distribuição, a data prevista, os Componentes pertinentes e a monetização. Isso inclui versões externas alpha, beta, de teste, de acesso antecipado e de lançamento completo. O desenvolvimento interno, por si só, não é publicação externa. Atualize o relatório em caso de alteração relevante da identidade do Produto, dos canais ou da monetização; atualizações comuns de versão não exigem uma licença negociada novamente.

9.5 Antes da primeira monetização, registre a aceitação das disposições comerciais das seções 10–11. Acesso alpha/beta pago, acesso antecipado e pré-vendas qualificadas não são adiados até um lançamento oficial. Um relatório e uma aceitação conformes não exigem que a Licenciadora exerça um veto discricionário sobre o uso, de outra forma permitido, de Componentes Públicos.

9.6 Você continua responsável pelo seu Produto, seus usuários, seu conteúdo e suas promessas aos clientes. Mantenha os avisos exigidos acessíveis em um local adequado de informações legais. Nada concede poderes para prometer hospedagem ou suporte em nome de Jan pessoalmente.

## 10. Tabela de participação sobre receita

10.1 A taxa de participação é de **5%** da Receita Sujeita à Participação. Cada Produto tem uma única **franquia de USD 100.000 de receita acumulada durante toda a sua existência**. Ela não reinicia anualmente, nem em caso de mudança de plataforma, atualização de versão, nova publicadora ou transferência do mesmo Produto para outra entidade. A receita acima da franquia é sujeita à participação; ultrapassá-la não sujeita retroativamente o valor isento.

10.2 Receita do Produto significa os recebimentos mundiais atribuíveis ao Produto: vendas, acesso pago, assinaturas, transações dentro do Produto, publicidade, alpha/beta pagos e acesso antecipado, e contraprestações de pré-vendas ou financiamento coletivo (crowdfunding) por acesso ao Produto ou benefícios. Adiantamentos relacionados ao Produto são computados na medida em que sejam atribuíveis a esses recebimentos, sem que o mesmo valor seja cobrado novamente na recuperação. Receitas de negócios não relacionados e doações genuínas sem contraprestação do Produto estão excluídas.

10.3 Calcule os recebimentos antes de comissões de plataforma, quotas de publicadoras e despesas de desenvolvimento ou de operação. Deduza os tributos sobre transações identificados separadamente e efetivamente devidos às autoridades públicas, bem como reembolsos genuínos a clientes. Estornos devem ser comprovados e alocados ao Produto, sem deduções em duplicidade. Um reembolso posterior de receita anteriormente sujeita à participação é creditado na conta de participação desse Produto; um crédito remanescente na liquidação final é reembolsável, e não tacitamente perdido.

10.4 Todas as versões de plataforma e atualizações do mesmo Produto são somadas. Produtos genuinamente independentes têm franquias independentes. Pacotes (bundles) e expansões devem ser alocados com base razoável, documentada e aplicada de forma consistente, que reflita a sua relação econômica. Não fragmente nem renomeie um Produto para multiplicar franquias. Incertezas relevantes de alocação devem ser divulgadas com o relatório.

10.5 Um trimestre civil com Receita do Produto **inferior a USD 1.000** não gera participação. Essa receita continua contando para a franquia de toda a existência. Com exatamente USD 1.000 ou mais, essa isenção trimestral não se aplica. Trata-se de um limite de isenção, e não de uma dedução adicional de USD 1.000.

10.6 Nos demais casos, a Receita Sujeita à Participação é a Receita do Produto do trimestre menos a franquia não utilizada no início do trimestre, mas nunca inferior a zero. A franquia não utilizada é igual a USD 100.000 menos a Receita do Produto acumulada anteriormente, mas nunca inferior a zero. Correções por reembolso devem ser conciliadas de modo que nem os recebimentos nem o uso da franquia sejam contados duas vezes.

10.7 Exemplos: com USD 80.000 de receita acumulada anterior e USD 30.000 no trimestre, a participação é de USD 500. Esgotada a franquia, um trimestre de USD 800 não gera participação; um trimestre de USD 2.000 gera USD 100. Estes exemplos pressupõem a ausência de correções posteriores.

10.8 As tarifas do backend são independentes da participação sobre receita e não são automaticamente compensadas com ela. Esta tabela não cria cobrança por instalação, nova taxa de participação, desconto especial de loja nem assinatura obrigatória adicional para código público.

## 11. Relatório, moeda e pagamento

11.1 A partir da primeira monetização, informe trimestralmente a Receita do Produto, as deduções, a receita acumulada, o uso da franquia e a participação em até **45 dias corridos após o fim do trimestre**. Pague qualquer participação no mesmo prazo. Um relatório sem pagamento deve mostrar por que nenhum pagamento é devido. Um Produto inativo pode ser marcado como inativo com o seu relatório final; informe a retomada da monetização quando ela ocorrer.

11.2 Guarde os demonstrativos pertinentes e os registros comprobatórios pelos prazos legais de retenção aplicáveis. A Licenciadora pode solicitar comprovação razoável de uma divergência identificada. Qualquer verificação deve proteger dados comerciais confidenciais e dados pessoais, limitar-se à receita pertinente e não exigir o código-fonte isento do seu Produto. Auditorias mais amplas exigem um procedimento acordado separadamente.

11.3 O USD é a unidade de referência para os limites e os cálculos. Para recebimentos em BRL, utilize a taxa PTAX de venda de fechamento USD/BRL do Banco Central do Brasil da data do recebimento ou a taxa do dia útil imediatamente anterior mais recente publicada. Para outra moeda, utilize uma taxa comprovada de liquidação efetiva em USD, quando disponível; caso contrário, utilize de forma consistente a conversão de referência correspondente publicada pelo banco central e divulgue a fonte e o método. Não escolha taxas de forma oportunista entre transações.

11.4 Para liquidação exigida em BRL, converta o valor em USD pela taxa de venda de fechamento do BCB do dia útil imediatamente anterior à emissão da fatura. A Licenciadora fornece, a tempo para o pagamento, instruções lícitas de faturamento e de pagamento. Exigências imperativas de câmbio, tributárias e de retenção na fonte prevalecem. A retenção legalmente exigida e comprovada por documentação oficial é considerada; este Contrato não impõe acréscimo automático de tributos (gross-up).

11.5 Envie os relatórios pelo canal privado de cliente designado ou pelo contato confidencial fornecido na integração (onboarding). Na ausência ou falha de um canal eletrônico, a notificação pode ser entregue na sede indicada na seção 1. Nunca divulgue receita privada em uma issue pública do GitHub. Uma falha de acesso não deve ser usada para fabricar inadimplemento quando a notificação tempestiva tiver sido razoavelmente tentada por uma alternativa disponível.

11.6 Corrija sem demora indevida os erros descobertos. O relatório atrasado não elimina automaticamente a franquia de toda a existência. Violações efetivas continuam sujeitas à seção 14 e à legislação aplicável. Um novo preço no site não cria participação retroativa.

## 12. Serviços PaaS e Enterprise

12.1 O PaaS adquire o fornecimento do backend e os recursos contratados de portal, banco de dados, API e hospedagem. Clientes padrão utilizam o portal e as interfaces autorizados; clientes Enterprise recebem adicionalmente os direitos especificados sobre código-fonte privado. O pedido vinculante identifica os recursos adquiridos, os preços, o período de cobrança, o suporte e o escopo do serviço. Este Contrato não é, em si, um pedido de hospedagem.

12.2 Disponibilidade do serviço, informações de validade técnica, tratamento de dados, exportação, backup e retenção devem ser informados nos termos de cliente e no pedido, conforme aplicável. Nenhuma licença privada ou compromisso de suporte é inferido apenas a partir da denominação de uma tarifa. Deveres imperativos relativos à comercialização de software não são afetados.

12.3 Um pedido pode limitar os direitos privados de desenvolvimento que fornece. Ele não deve extinguir as permissões públicas da seção 4 apenas porque uma assinatura termina. Após o cancelamento, o cliente continua autorizado a desenvolver um Produto de outra forma conforme utilizando apenas Componentes Públicos licenciados de forma independente.

## 13. Cancelamento, redução de plano e publicações existentes

13.1 Você pode solicitar a redução de plano a qualquer momento, com efeito ao final do período de cobrança vigente acordado. Um período mensal termina no fim do mês acordado e um período anual no fim do ano acordado. O pedido e a legislação imperativa regem o aviso de rescisão e eventual reembolso; a possibilidade de solicitar a redução de plano não encurta o período adquirido.

13.2 Na redução de plano, aplicam-se os direitos e recursos do novo plano. Os direitos privados que deixarem de estar incluídos cessam para o futuro. No cancelamento ou na suspensão, o backend e o acesso afetados cessam conforme previsto no contrato. A posse de um checkout privado não gera direitos continuados de desenvolvimento nem uma licença de auto-hospedagem.

13.3 Você pode continuar distribuindo a versão inalterada e licitamente publicada do Produto dentro dos direitos de distribuição remanescentes. Os usuários finais podem continuar o seu uso autorizado em tempo de execução. A participação sobre receita e o relatório continuam aplicáveis. Isso não promete a continuidade dos serviços de backend: um Produto offline pode funcionar de forma independente, ao passo que um Produto dependente do backend pode deixar de funcionar.

13.4 Você não pode criar um novo Produto ou uma nova versão utilizando direitos privados de desenvolvimento vencidos, incluindo uma suposta atualização de manutenção que necessite desses direitos. Você pode retomar com uma autorização adequada ou desenvolver dentro dos direitos que ainda detém. O trabalho que utilize apenas Componentes Públicos licitamente disponíveis continua permitido sem assinatura, nos termos da seção 4.

13.5 Guarde cópias privadas apenas na medida permitida para os direitos de distribuição remanescentes, para registros exigidos ou para arquivos autorizados. A guarda não autoriza novo desenvolvimento. Aplique os deveres pertinentes de devolução/exclusão sem destruir registros exigidos pela legislação imperativa. O término da assinatura ou da condição de Membro não retira direitos já concedidos sobre Contribuições.

## 14. Violação, garantias e responsabilidade

14.1 A Licenciadora pode exigir a cessação e a correção de uma violação identificada. Uma violação sanável normalmente recebe uma oportunidade razoável de saneamento, indicada com a notificação e proporcional à sua gravidade e ao trabalho necessário. A suspensão imediata e direcionada do acesso pode ser utilizada quando necessária em caso de divulgação não autorizada grave, infração ou riscos de segurança. A rescisão por justa causa segue a legislação aplicável; um pequeno erro técnico não rescinde automaticamente todas as licenças.

14.2 Contribuições, confidencialidade, deveres de relatório/pagamento já constituídos e direitos válidos existentes de terceiros subsistem na extensão dos seus termos. A seção 13 não é uma garantia de que um Produto distribuído ilicitamente possa continuar após rescisão por infração grave.

14.3 Os materiais gratuitos de desenvolvimento são fornecidos no estado em que se encontram, sem promessa contratual de adequação, conclusão ou funcionamento sem erros, salvo se expressamente dada. Promessas de serviços pagos e direitos imperativos não são afetados. Você deve avaliar a adequação ao Produto pretendido.

14.4 Na medida permitida por lei, exclui-se a responsabilidade por perdas indiretas ou consequenciais e por lucros cessantes. Isso não exclui fraude, conduta dolosa ou gravemente culposa, danos pessoais, uma garantia vinculante, responsabilidade imperativa ou responsabilidade que não possa ser licitamente excluída quanto a deveres contratuais essenciais. Isso não se sobrepõe à proteção do consumidor. Nenhum limite financeiro arbitrário de responsabilidade é imposto aqui.

14.5 Jan não garante pessoalmente as dívidas ou os contratos de clientes da sociedade, e nenhum dever de prestação de serviço é transferido automaticamente a ele em caso de falência da sociedade. Cada parte continua responsável pela sua própria conduta juridicamente imputável. Este Contrato não pode impedir todas as reclamações de terceiros nem afastar a responsabilidade legal imperativa.

## 15. Versões e continuidade do projeto

15.1 Jan Ohlmann é o emissor designado de futuras versões da LIC. Cada uma deve ter identificador, data e registro de alterações. As permissões relativas às contribuições incluem o poder de continuar e sublicenciar as Contribuições efetivamente submetidas sob versões posteriores emitidas por ele que preservem o desenvolvimento compartilhado da Engine, a disponibilidade aos destinatários autorizados da comunidade, a exclusão do Produto independente e os benefícios de contribuição já concedidos. A assinatura adicional de um contribuidor não é necessária para usos dentro desse poder validamente concedido.

15.2 Esse poder não impõe novos deveres arbitrários de pagamento, responsabilidade ou prestação aos usuários existentes. Alterações dos seus contratos exigem base legal. O silêncio ou o commit de outra pessoa não é consentimento geral. Permissões já concedidas e licenças independentes de terceiros não podem ser retiradas retroativamente pela substituição de um arquivo.

15.3 O ADG e os direitos válidos sobre Contribuições não se dissolvem simplesmente porque uma sociedade operadora encerra suas atividades. Mudanças de controle da sociedade não transmitem a titularidade privada de Jan nem um direito automático a versões futuras. A licença superior separada e as regras imperativas de insolvência regem a posição da sociedade. Outro operador precisa de poderes próprios; este Contrato não confere direito geral de franquia.

## 16. PJV, notificações e lei aplicável

16.1 Os contribuidores também devem aceitar o PJV identificado. A LIC rege os direitos sobre contribuições, os termos de participação sobre receita, as permissões públicas/privadas e os limites do Produto; o PJV rege a participação e o fluxo de trabalho do projeto. Nenhum dos documentos altera tacitamente o contrato social da sociedade. Para contribuições aceitas a partir destas versões, as regras incompatíveis de retirada e de troca automática de licença da LIC/PJV de 2016 não se aplicam. Isso não reescreve retroativamente um contrato antigo apenas pela publicação.

16.2 Termos individuais podem dispor sobre matérias dentro dos poderes efetivos das partes, mas não podem dispor dos direitos de outro titular. O tratamento de dados pessoais exige base legal própria e as informações de privacidade necessárias. Isso não substitui um contrato de tratamento de dados quando este for exigido.

16.3 As notificações devem ser razoavelmente comprovadas por escrito ou em texto eletrônico, pelo canal contratual privado pertinente ou na sede registrada. Questões comuns de código público podem usar os canais do repositório. A aprovação técnica do projeto não é uma garantia pessoal nem uma renúncia a termos pecuniários.

16.4 Aplica-se a lei brasileira na medida em que a escolha de lei seja eficaz. Proteções imperativas aplicáveis a uma pessoa ou transação e a competência judicial irrenunciável continuam aplicáveis. Esta escolha de lei não priva o consumidor da proteção conferida pelas disposições imperativas da lei do Estado de sua residência habitual. Quando um acordo de foro for lícito, fica designado o foro de Caucaia, Ceará, Brasil. Não se impõe exigência de arbitragem nem renúncia a meios coletivos imperativos de tutela.

16.5 Este Contrato é disponibilizado em português do Brasil (pt-BR), inglês e alemão, com o mesmo conteúdo. A versão em pt-BR é a determinante; havendo divergência entre as versões linguísticas, prevalece a versão em pt-BR. As versões em inglês e em alemão são sempre fornecidas juntamente com a versão em pt-BR antes da contratação e publicadas com ela no site da Licenciadora. Quando a legislação aplicável exigir informações pré-contratuais em português ou de outra forma compreensíveis, a Licenciadora deve fornecê-las antes da contratação. Nenhuma prioridade linguística afasta deveres imperativos de interpretação ou de informação, incluindo as normas de proteção do consumidor do Estado de residência habitual. Uma tradução deve identificar a sua versão correspondente e eventual status prevalente expressamente acordado.

16.6 Se uma cláusula for inválida, as demais disposições subsistem apenas na medida permitida por lei. Uma exclusão inválida não é automaticamente reescrita com o máximo efeito restritivo possível. Aplicam-se as regras legais de substituição cabíveis.

16.7 Esta versão torna-se uma oferta para um Componente somente com a publicação autorizada, depois de a Licenciadora dispor dos direitos necessários. A sua data de emissão não é uma afirmação de que um contrato superior já tenha sido assinado. Cópias inalteradas podem acompanhar o material licenciado. Modificações não aprovadas não devem ser apresentadas como licença oficial da Antares.

Direitos autorais sobre o texto da licença: © 2026 Jan Ohlmann. Os direitos autorais sobre os materiais licenciados permanecem com os respectivos titulares.
